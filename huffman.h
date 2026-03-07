#pragma once
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <sstream>
#include "crow.h"

struct HuffmanNode {
    int byteValue;
    int frequency;
    HuffmanNode* left;
    HuffmanNode* right;

    HuffmanNode(int val, int freq) : byteValue(val), frequency(freq), left(nullptr), right(nullptr) {}
    HuffmanNode(HuffmanNode* l, HuffmanNode* r)
        : byteValue(-1), frequency((l ? l->frequency : 0) + (r ? r->frequency : 0)), left(l), right(r) {}
    bool isLeaf() const { return !left && !right; }
};

struct HuffmanCompare {
    bool operator()(HuffmanNode* a, HuffmanNode* b) { return a->frequency > b->frequency; }
};

struct HuffmanResult {
    std::vector<uint8_t> compressedData;
    crow::json::wvalue steps;
};

struct HuffmanDecompressResult {
    std::vector<uint8_t> decompressedData;
    crow::json::wvalue steps;
};

inline void freeTree(HuffmanNode* node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

inline void generateCodes(HuffmanNode* node, const std::string& code, std::string codes[256]) {
    if (!node) return;
    if (node->isLeaf()) {
        codes[node->byteValue] = code.empty() ? "0" : code;
        return;
    }
    generateCodes(node->left, code + "0", codes);
    generateCodes(node->right, code + "1", codes);
}

inline void treeToJson(HuffmanNode* node, crow::json::wvalue& out) {
    if (!node) return;
    out["frequency"] = node->frequency;
    if (node->isLeaf()) {
        out["byte"] = node->byteValue;
        if (node->byteValue >= 32 && node->byteValue < 127)
            out["char"] = std::string(1, (char)node->byteValue);
        else
            out["char"] = "0x" + ([](int v) {
                char buf[8]; snprintf(buf, sizeof(buf), "%02X", v); return std::string(buf);
            })(node->byteValue);
    } else {
        crow::json::wvalue leftJson, rightJson;
        treeToJson(node->left, leftJson);
        treeToJson(node->right, rightJson);
        out["left"] = std::move(leftJson);
        out["right"] = std::move(rightJson);
    }
}

inline HuffmanResult huffmanCompress(const std::vector<uint8_t>& input) {
    HuffmanResult result;

    if (input.empty()) {
        result.steps["error"] = "Empty input";
        return result;
    }

    // Step 1: Calculate frequencies
    int freq[256] = {0};
    for (uint8_t b : input) freq[b]++;

    std::vector<crow::json::wvalue> freqSteps;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            crow::json::wvalue entry;
            entry["byte"] = i;
            if (i >= 32 && i < 127)
                entry["char"] = std::string(1, (char)i);
            else
                entry["char"] = "0x" + ([](int v) {
                    char buf[8]; snprintf(buf, sizeof(buf), "%02X", v); return std::string(buf);
                })(i);
            entry["frequency"] = freq[i];
            freqSteps.push_back(std::move(entry));
        }
    }
    result.steps["frequencies"] = std::move(freqSteps);

    // Step 2: Build Huffman tree with merge steps
    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, HuffmanCompare> pq;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) pq.push(new HuffmanNode(i, freq[i]));
    }

    int uniqueCount = pq.size();
    std::vector<crow::json::wvalue> mergeSteps;

    if (uniqueCount == 0) {
        result.steps["error"] = "No data to compress";
        return result;
    }

    if (uniqueCount == 1) {
        HuffmanNode* only = pq.top(); pq.pop();
        HuffmanNode* root = new HuffmanNode(only, nullptr);

        crow::json::wvalue mergeStep;
        mergeStep["type"] = "single";
        mergeStep["byte"] = only->byteValue;
        mergeStep["frequency"] = only->frequency;
        mergeSteps.push_back(std::move(mergeStep));

        result.steps["mergeSteps"] = std::move(mergeSteps);

        std::string codes[256];
        generateCodes(root, "", codes);

        // Build code table for visualization
        std::vector<crow::json::wvalue> codeTable;
        crow::json::wvalue ce;
        ce["byte"] = only->byteValue;
        ce["code"] = codes[only->byteValue];
        codeTable.push_back(std::move(ce));
        result.steps["codeTable"] = std::move(codeTable);

        crow::json::wvalue treeJson;
        treeToJson(root, treeJson);
        result.steps["tree"] = std::move(treeJson);

        // Encode
        std::string bitString;
        for (uint8_t b : input) bitString += codes[b];

        int extraBits = (8 - (bitString.size() % 8)) % 8;
        for (int i = 0; i < extraBits; i++) bitString += "0";

        // Header: uniqueCount(4) + freq pairs(5 each) + extraBits(1)
        // Write unique count
        uint32_t uc = uniqueCount;
        result.compressedData.push_back((uc >> 24) & 0xFF);
        result.compressedData.push_back((uc >> 16) & 0xFF);
        result.compressedData.push_back((uc >> 8) & 0xFF);
        result.compressedData.push_back(uc & 0xFF);

        // Write freq table
        for (int i = 0; i < 256; i++) {
            if (freq[i] > 0) {
                result.compressedData.push_back((uint8_t)i);
                result.compressedData.push_back((freq[i] >> 24) & 0xFF);
                result.compressedData.push_back((freq[i] >> 16) & 0xFF);
                result.compressedData.push_back((freq[i] >> 8) & 0xFF);
                result.compressedData.push_back(freq[i] & 0xFF);
            }
        }

        // Write extra bits count
        result.compressedData.push_back((uint8_t)extraBits);

        // Write compressed data
        for (size_t i = 0; i < bitString.size(); i += 8) {
            uint8_t byte = 0;
            for (int j = 0; j < 8; j++) byte = (byte << 1) | (bitString[i + j] - '0');
            result.compressedData.push_back(byte);
        }

        // Encoding steps (sample)
        std::vector<crow::json::wvalue> encSteps;
        size_t sampleSize = std::min(input.size(), (size_t)50);
        for (size_t i = 0; i < sampleSize; i++) {
            crow::json::wvalue es;
            es["byte"] = input[i];
            es["code"] = codes[input[i]];
            es["position"] = (int)i;
            encSteps.push_back(std::move(es));
        }
        result.steps["encodingSteps"] = std::move(encSteps);

        freeTree(root);
        return result;
    }

    // Normal case: multiple unique bytes
    while (pq.size() > 1) {
        HuffmanNode* left = pq.top(); pq.pop();
        HuffmanNode* right = pq.top(); pq.pop();
        HuffmanNode* parent = new HuffmanNode(left, right);
        pq.push(parent);

        crow::json::wvalue mergeStep;
        mergeStep["type"] = "merge";
        mergeStep["leftFreq"] = left->frequency;
        mergeStep["rightFreq"] = right->frequency;
        mergeStep["parentFreq"] = parent->frequency;
        if (left->isLeaf()) mergeStep["leftByte"] = left->byteValue;
        if (right->isLeaf()) mergeStep["rightByte"] = right->byteValue;
        mergeSteps.push_back(std::move(mergeStep));
    }
    result.steps["mergeSteps"] = std::move(mergeSteps);

    HuffmanNode* root = pq.top(); pq.pop();

    // Generate codes
    std::string codes[256];
    generateCodes(root, "", codes);

    std::vector<crow::json::wvalue> codeTable;
    for (int i = 0; i < 256; i++) {
        if (!codes[i].empty()) {
            crow::json::wvalue ce;
            ce["byte"] = i;
            if (i >= 32 && i < 127)
                ce["char"] = std::string(1, (char)i);
            else
                ce["char"] = "0x" + ([](int v) {
                    char buf[8]; snprintf(buf, sizeof(buf), "%02X", v); return std::string(buf);
                })(i);
            ce["code"] = codes[i];
            ce["frequency"] = freq[i];
            codeTable.push_back(std::move(ce));
        }
    }
    result.steps["codeTable"] = std::move(codeTable);

    crow::json::wvalue treeJson;
    treeToJson(root, treeJson);
    result.steps["tree"] = std::move(treeJson);

    // Encode
    std::string bitString;
    for (uint8_t b : input) bitString += codes[b];

    int extraBits = (8 - (bitString.size() % 8)) % 8;
    for (int i = 0; i < extraBits; i++) bitString += "0";

    // Write header
    uint32_t uc = uniqueCount;
    result.compressedData.push_back((uc >> 24) & 0xFF);
    result.compressedData.push_back((uc >> 16) & 0xFF);
    result.compressedData.push_back((uc >> 8) & 0xFF);
    result.compressedData.push_back(uc & 0xFF);

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            result.compressedData.push_back((uint8_t)i);
            result.compressedData.push_back((freq[i] >> 24) & 0xFF);
            result.compressedData.push_back((freq[i] >> 16) & 0xFF);
            result.compressedData.push_back((freq[i] >> 8) & 0xFF);
            result.compressedData.push_back(freq[i] & 0xFF);
        }
    }

    result.compressedData.push_back((uint8_t)extraBits);

    for (size_t i = 0; i < bitString.size(); i += 8) {
        uint8_t byte = 0;
        for (int j = 0; j < 8; j++) byte = (byte << 1) | (bitString[i + j] - '0');
        result.compressedData.push_back(byte);
    }

    // Encoding steps (sample first 50 chars for visualization)
    std::vector<crow::json::wvalue> encSteps;
    size_t sampleSize = std::min(input.size(), (size_t)50);
    for (size_t i = 0; i < sampleSize; i++) {
        crow::json::wvalue es;
        es["byte"] = input[i];
        if (input[i] >= 32 && input[i] < 127)
            es["char"] = std::string(1, (char)input[i]);
        es["code"] = codes[input[i]];
        es["position"] = (int)i;
        encSteps.push_back(std::move(es));
    }
    result.steps["encodingSteps"] = std::move(encSteps);
    result.steps["totalBits"] = (int)bitString.size();
    result.steps["originalSize"] = (int)input.size();
    result.steps["compressedSize"] = (int)result.compressedData.size();

    freeTree(root);
    return result;
}

inline HuffmanDecompressResult huffmanDecompress(const std::vector<uint8_t>& data) {
    HuffmanDecompressResult result;

    if (data.size() < 5) {
        result.steps["error"] = "Invalid compressed data";
        return result;
    }

    size_t pos = 0;

    // Read unique count
    uint32_t uniqueCount = ((uint32_t)data[pos] << 24) | ((uint32_t)data[pos+1] << 16) |
                           ((uint32_t)data[pos+2] << 8) | data[pos+3];
    pos += 4;

    // Read frequency table
    int freq[256] = {0};
    std::vector<crow::json::wvalue> freqSteps;
    for (uint32_t i = 0; i < uniqueCount && pos + 4 < data.size(); i++) {
        uint8_t byteVal = data[pos++];
        int f = ((int)data[pos] << 24) | ((int)data[pos+1] << 16) | ((int)data[pos+2] << 8) | data[pos+3];
        pos += 4;
        freq[byteVal] = f;

        crow::json::wvalue entry;
        entry["byte"] = byteVal;
        entry["frequency"] = f;
        freqSteps.push_back(std::move(entry));
    }
    result.steps["frequencies"] = std::move(freqSteps);

    // Read extra bits
    int extraBits = data[pos++];
    result.steps["extraBits"] = extraBits;

    // Rebuild tree
    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, HuffmanCompare> pq;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) pq.push(new HuffmanNode(i, freq[i]));
    }

    HuffmanNode* root = nullptr;
    if (pq.size() == 1) {
        HuffmanNode* only = pq.top(); pq.pop();
        root = new HuffmanNode(only, nullptr);
    } else {
        while (pq.size() > 1) {
            HuffmanNode* l = pq.top(); pq.pop();
            HuffmanNode* r = pq.top(); pq.pop();
            pq.push(new HuffmanNode(l, r));
        }
        if (!pq.empty()) { root = pq.top(); pq.pop(); }
    }

    if (!root) {
        result.steps["error"] = "Failed to rebuild tree";
        return result;
    }

    crow::json::wvalue treeJson;
    treeToJson(root, treeJson);
    result.steps["tree"] = std::move(treeJson);

    // Convert remaining bytes to bit string
    std::string bitString;
    for (size_t i = pos; i < data.size(); i++) {
        for (int j = 7; j >= 0; j--) bitString += ((data[i] >> j) & 1) ? '1' : '0';
    }
    if (extraBits > 0 && bitString.size() >= (size_t)extraBits)
        bitString.resize(bitString.size() - extraBits);

    // Calculate total original bytes
    int totalBytes = 0;
    for (int i = 0; i < 256; i++) totalBytes += freq[i];

    // Decode
    std::vector<crow::json::wvalue> decodeSteps;
    HuffmanNode* current = root;
    int decoded = 0;
    std::string currentCode;
    for (size_t i = 0; i < bitString.size() && decoded < totalBytes; i++) {
        currentCode += bitString[i];
        if (bitString[i] == '0') current = current->left;
        else current = current->right;

        if (current && current->isLeaf()) {
            result.decompressedData.push_back((uint8_t)current->byteValue);
            if (decoded < 50) {
                crow::json::wvalue ds;
                ds["code"] = currentCode;
                ds["byte"] = current->byteValue;
                ds["position"] = decoded;
                decodeSteps.push_back(std::move(ds));
            }
            decoded++;
            currentCode.clear();
            current = root;
        }
    }
    result.steps["decodeSteps"] = std::move(decodeSteps);
    result.steps["decompressedSize"] = (int)result.decompressedData.size();

    freeTree(root);
    return result;
}
