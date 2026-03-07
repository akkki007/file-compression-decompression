#include "crow.h"
#include "huffman.h"
#include "lzw.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// Base64 encoding for sending binary data as JSON
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<uint8_t>& data) {
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3], char_array_4[4];
    size_t in_len = data.size();
    const uint8_t* bytes_to_encode = data.data();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for (int j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        for (int j = 0; j < i + 1; j++) ret += base64_chars[char_array_4[j]];
        while (i++ < 3) ret += '=';
    }
    return ret;
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    std::vector<uint8_t> ret;
    int i = 0;
    unsigned char char_array_4[4], char_array_3[3];

    auto isBase64 = [](unsigned char c) {
        return (isalnum(c) || c == '+' || c == '/');
    };

    size_t in_len = encoded.size();
    size_t in_pos = 0;

    while (in_len-- && encoded[in_pos] != '=') {
        if (!isBase64(encoded[in_pos])) { in_pos++; continue; }
        char_array_4[i++] = encoded[in_pos]; in_pos++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                const char* p = strchr(base64_chars, char_array_4[i]);
                char_array_4[i] = p ? (unsigned char)(p - base64_chars) : 0;
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; i < 3; i++) ret.push_back(char_array_3[i]);
            i = 0;
        }
    }
    if (i) {
        for (int j = i; j < 4; j++) char_array_4[j] = 0;
        for (int j = 0; j < 4; j++) {
            const char* p = strchr(base64_chars, char_array_4[j]);
            char_array_4[j] = p ? (unsigned char)(p - base64_chars) : 0;
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        if (i >= 2) ret.push_back(char_array_3[0]);
        if (i >= 3) ret.push_back(char_array_3[1]);
    }
    return ret;
}

struct CORSHandler {
    struct context {};

    void before_handle(crow::request& /*req*/, crow::response& /*res*/, context& /*ctx*/) {}

    void after_handle(crow::request& /*req*/, crow::response& res, context& /*ctx*/) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    }
};

int main() {
    crow::App<CORSHandler> app;

    // Serve React frontend
    CROW_ROUTE(app, "/")
    ([]() {
        std::ifstream file("frontend/build/index.html");
        if (!file.is_open()) {
            return crow::response(200,
                "<html><body><h1>File Compression Server Running</h1>"
                "<p>Build the React frontend with: cd frontend && npm run build</p></body></html>");
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto resp = crow::response(200, content);
        resp.set_header("Content-Type", "text/html");
        return resp;
    });

    // POST + OPTIONS /api/compress
    CROW_ROUTE(app, "/api/compress").methods("POST"_method, "OPTIONS"_method)
    ([](const crow::request& req) {
        // Handle CORS preflight
        if (req.method == "OPTIONS"_method) {
            auto resp = crow::response(204);
            resp.set_header("Access-Control-Allow-Origin", "*");
            resp.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            resp.set_header("Access-Control-Allow-Headers", "Content-Type");
            return resp;
        }

        auto body = crow::json::load(req.body);
        if (!body) {
            crow::json::wvalue err;
            err["error"] = "Invalid JSON";
            auto resp = crow::response(400, err.dump());
            resp.set_header("Content-Type", "application/json");
            resp.set_header("Access-Control-Allow-Origin", "*");
            return resp;
        }

        std::string algorithm = body["algorithm"].s();
        std::string dataB64 = body["data"].s();
        std::string filename = "file";
        if (body.has("filename")) filename = body["filename"].s();

        std::vector<uint8_t> inputData = base64Decode(dataB64);

        if (inputData.empty()) {
            crow::json::wvalue err;
            err["error"] = "Empty file data";
            auto resp = crow::response(400, err.dump());
            resp.set_header("Content-Type", "application/json");
            resp.set_header("Access-Control-Allow-Origin", "*");
            return resp;
        }

        crow::json::wvalue response;
        response["algorithm"] = algorithm;
        response["originalSize"] = (int)inputData.size();
        response["filename"] = filename;

        if (algorithm == "huffman") {
            auto result = huffmanCompress(inputData);
            response["compressedData"] = base64Encode(result.compressedData);
            response["compressedSize"] = (int)result.compressedData.size();
            response["compressionRatio"] = inputData.empty() ? 0.0 :
                (double)result.compressedData.size() / inputData.size() * 100.0;
            response["steps"] = std::move(result.steps);
        } else if (algorithm == "lzw") {
            auto result = lzwCompress(inputData);
            response["compressedData"] = base64Encode(result.compressedData);
            response["compressedSize"] = (int)result.compressedData.size();
            response["compressionRatio"] = inputData.empty() ? 0.0 :
                (double)result.compressedData.size() / inputData.size() * 100.0;
            response["steps"] = std::move(result.steps);
        } else {
            crow::json::wvalue err;
            err["error"] = "Unknown algorithm. Use 'huffman' or 'lzw'";
            auto resp = crow::response(400, err.dump());
            resp.set_header("Content-Type", "application/json");
            resp.set_header("Access-Control-Allow-Origin", "*");
            return resp;
        }

        auto resp = crow::response(200, response.dump());
        resp.set_header("Content-Type", "application/json");
        resp.set_header("Access-Control-Allow-Origin", "*");
        return resp;
    });

    // POST + OPTIONS /api/decompress
    CROW_ROUTE(app, "/api/decompress").methods("POST"_method, "OPTIONS"_method)
    ([](const crow::request& req) {
        // Handle CORS preflight
        if (req.method == "OPTIONS"_method) {
            auto resp = crow::response(204);
            resp.set_header("Access-Control-Allow-Origin", "*");
            resp.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            resp.set_header("Access-Control-Allow-Headers", "Content-Type");
            return resp;
        }

        auto body = crow::json::load(req.body);
        if (!body) {
            crow::json::wvalue err;
            err["error"] = "Invalid JSON";
            auto resp = crow::response(400, err.dump());
            resp.set_header("Content-Type", "application/json");
            resp.set_header("Access-Control-Allow-Origin", "*");
            return resp;
        }

        std::string algorithm = body["algorithm"].s();
        std::string dataB64 = body["data"].s();

        std::vector<uint8_t> compressedData = base64Decode(dataB64);

        crow::json::wvalue response;
        response["algorithm"] = algorithm;

        if (algorithm == "huffman") {
            auto result = huffmanDecompress(compressedData);
            response["decompressedData"] = base64Encode(result.decompressedData);
            response["decompressedSize"] = (int)result.decompressedData.size();
            response["steps"] = std::move(result.steps);
        } else if (algorithm == "lzw") {
            auto result = lzwDecompress(compressedData);
            response["decompressedData"] = base64Encode(result.decompressedData);
            response["decompressedSize"] = (int)result.decompressedData.size();
            response["steps"] = std::move(result.steps);
        } else {
            crow::json::wvalue err;
            err["error"] = "Unknown algorithm. Use 'huffman' or 'lzw'";
            auto resp = crow::response(400, err.dump());
            resp.set_header("Content-Type", "application/json");
            resp.set_header("Access-Control-Allow-Origin", "*");
            return resp;
        }

        auto resp = crow::response(200, response.dump());
        resp.set_header("Content-Type", "application/json");
        resp.set_header("Access-Control-Allow-Origin", "*");
        return resp;
    });

    std::cout << "=== File Compression Server ===" << std::endl;
    std::cout << "Backend: http://localhost:18080" << std::endl;
    std::cout << "API: POST /api/compress, POST /api/decompress" << std::endl;

    app.port(18080).multithreaded().run();
    return 0;
}

