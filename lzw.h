#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "crow.h"

static const int LZW_MAX_DICT_SIZE = 65536;

struct LzwResult {
    std::vector<uint8_t> compressedData;
    crow::json::wvalue steps;
};

struct LzwDecompressResult {
    std::vector<uint8_t> decompressedData;
    crow::json::wvalue steps;
};

inline int calculateBitSize(int dictSize) {
    if (dictSize <= 1) return 1;
    int bits = 0;
    int v = dictSize - 1;
    while (v > 0) { bits++; v >>= 1; }
    return bits;
}

inline LzwResult lzwCompress(const std::vector<uint8_t>& input) {
    LzwResult result;

    if (input.empty()) {
        result.steps["error"] = "Empty input";
        return result;
    }

    // First pass: calculate dictionary size and bit size
    std::unordered_map<std::string, int> dict;
    for (int i = 0; i < 256; i++) dict[std::string(1, (char)i)] = i;

    int dictSize = 256;
    std::string current(1, (char)input[0]);

    // First pass to determine final dict size
    {
        std::unordered_map<std::string, int> tmpDict = dict;
        int tmpSize = 256;
        std::string tmp(1, (char)input[0]);
        for (size_t i = 1; i < input.size(); i++) {
            std::string next = tmp + (char)input[i];
            if (tmpDict.count(next)) {
                tmp = next;
            } else {
                if (tmpSize < LZW_MAX_DICT_SIZE) {
                    tmpDict[next] = tmpSize++;
                }
                tmp = std::string(1, (char)input[i]);
            }
        }
        dictSize = tmpSize;
    }

    int bitSize = calculateBitSize(dictSize);

    // Step tracking for visualization
    std::vector<crow::json::wvalue> dictSteps;
    std::vector<crow::json::wvalue> encodeSteps;

    // Reset dictionary for actual compression
    dict.clear();
    for (int i = 0; i < 256; i++) dict[std::string(1, (char)i)] = i;
    int currentDictSize = 256;

    // Write header: bitSize (4 bytes)
    result.compressedData.push_back((bitSize >> 24) & 0xFF);
    result.compressedData.push_back((bitSize >> 16) & 0xFF);
    result.compressedData.push_back((bitSize >> 8) & 0xFF);
    result.compressedData.push_back(bitSize & 0xFF);

    std::string bitBuffer;
    std::vector<int> codes;

    current = std::string(1, (char)input[0]);
    int stepCount = 0;

    for (size_t i = 1; i < input.size(); i++) {
        std::string next = current + (char)input[i];
        if (dict.count(next)) {
            current = next;
        } else {
            int code = dict[current];
            codes.push_back(code);

            // Convert code to fixed-length binary
            for (int b = bitSize - 1; b >= 0; b--)
                bitBuffer += ((code >> b) & 1) ? '1' : '0';

            // Flush complete bytes
            while (bitBuffer.size() >= 8) {
                uint8_t byte = 0;
                for (int j = 0; j < 8; j++) byte = (byte << 1) | (bitBuffer[j] - '0');
                result.compressedData.push_back(byte);
                bitBuffer = bitBuffer.substr(8);
            }

            // Track steps for visualization (limit to 80)
            if (stepCount < 80) {
                crow::json::wvalue es;
                es["step"] = stepCount;
                es["sequence"] = current;
                es["code"] = code;
                es["newEntry"] = next;
                es["newCode"] = currentDictSize;
                encodeSteps.push_back(std::move(es));

                if (currentDictSize < LZW_MAX_DICT_SIZE) {
                    crow::json::wvalue ds;
                    ds["index"] = currentDictSize;
                    ds["entry"] = next;
                    dictSteps.push_back(std::move(ds));
                }
            }

            if (currentDictSize < LZW_MAX_DICT_SIZE) {
                dict[next] = currentDictSize++;
            }
            current = std::string(1, (char)input[i]);
            stepCount++;
        }
    }

    // Output last code
    if (!current.empty()) {
        int code = dict[current];
        codes.push_back(code);
        for (int b = bitSize - 1; b >= 0; b--)
            bitBuffer += ((code >> b) & 1) ? '1' : '0';
        while (bitBuffer.size() >= 8) {
            uint8_t byte = 0;
            for (int j = 0; j < 8; j++) byte = (byte << 1) | (bitBuffer[j] - '0');
            result.compressedData.push_back(byte);
            bitBuffer = bitBuffer.substr(8);
        }
    }

    // Flush remaining bits
    if (!bitBuffer.empty()) {
        while (bitBuffer.size() < 8) bitBuffer += '0';
        uint8_t byte = 0;
        for (int j = 0; j < 8; j++) byte = (byte << 1) | (bitBuffer[j] - '0');
        result.compressedData.push_back(byte);
    }

    // Write total codes count after header (4 bytes at position 4)
    uint32_t totalCodes = codes.size();
    // We need to insert this after the bitSize header - restructure
    // Actually let's prepend totalCodes right after bitSize
    std::vector<uint8_t> finalData;
    // bitSize (4 bytes)
    finalData.push_back((bitSize >> 24) & 0xFF);
    finalData.push_back((bitSize >> 16) & 0xFF);
    finalData.push_back((bitSize >> 8) & 0xFF);
    finalData.push_back(bitSize & 0xFF);
    // totalCodes (4 bytes)
    finalData.push_back((totalCodes >> 24) & 0xFF);
    finalData.push_back((totalCodes >> 16) & 0xFF);
    finalData.push_back((totalCodes >> 8) & 0xFF);
    finalData.push_back(totalCodes & 0xFF);
    // compressed data (skip original 4-byte header)
    for (size_t i = 4; i < result.compressedData.size(); i++)
        finalData.push_back(result.compressedData[i]);
    result.compressedData = finalData;

    result.steps["encodeSteps"] = std::move(encodeSteps);
    result.steps["dictionarySteps"] = std::move(dictSteps);
    result.steps["bitSize"] = bitSize;
    result.steps["finalDictSize"] = currentDictSize;
    result.steps["totalCodes"] = (int)totalCodes;
    result.steps["originalSize"] = (int)input.size();
    result.steps["compressedSize"] = (int)result.compressedData.size();

    return result;
}

inline LzwDecompressResult lzwDecompress(const std::vector<uint8_t>& data) {
    LzwDecompressResult result;

    if (data.size() < 8) {
        result.steps["error"] = "Invalid compressed data";
        return result;
    }

    size_t pos = 0;

    // Read bitSize
    int bitSize = ((int)data[pos] << 24) | ((int)data[pos+1] << 16) |
                  ((int)data[pos+2] << 8) | data[pos+3];
    pos += 4;

    // Read totalCodes
    uint32_t totalCodes = ((uint32_t)data[pos] << 24) | ((uint32_t)data[pos+1] << 16) |
                          ((uint32_t)data[pos+2] << 8) | data[pos+3];
    pos += 4;

    result.steps["bitSize"] = bitSize;
    result.steps["totalCodes"] = (int)totalCodes;

    // Convert remaining bytes to bit string
    std::string bitString;
    for (size_t i = pos; i < data.size(); i++) {
        for (int j = 7; j >= 0; j--) bitString += ((data[i] >> j) & 1) ? '1' : '0';
    }

    // Initialize decompression dictionary
    std::unordered_map<int, std::string> dict;
    for (int i = 0; i < 256; i++) dict[i] = std::string(1, (char)i);
    int dictSize = 256;

    std::vector<crow::json::wvalue> decodeSteps;

    // Read codes from bit string
    auto readCode = [&](size_t& bitPos) -> int {
        if (bitPos + bitSize > bitString.size()) return -1;
        int code = 0;
        for (int i = 0; i < bitSize; i++)
            code = (code << 1) | (bitString[bitPos + i] - '0');
        bitPos += bitSize;
        return code;
    };

    size_t bitPos = 0;
    int prevCode = readCode(bitPos);
    if (prevCode < 0 || !dict.count(prevCode)) {
        result.steps["error"] = "Invalid first code";
        return result;
    }

    std::string prevString = dict[prevCode];
    for (char c : prevString) result.decompressedData.push_back((uint8_t)c);

    int stepCount = 0;
    for (uint32_t i = 1; i < totalCodes; i++) {
        int code = readCode(bitPos);
        if (code < 0) break;

        std::string entry;
        if (dict.count(code)) {
            entry = dict[code];
        } else if (code == dictSize) {
            entry = prevString + prevString[0];
        } else {
            result.steps["error"] = "Invalid code during decompression";
            return result;
        }

        for (char c : entry) result.decompressedData.push_back((uint8_t)c);

        if (dictSize < LZW_MAX_DICT_SIZE) {
            dict[dictSize] = prevString + entry[0];
            if (stepCount < 80) {
                crow::json::wvalue ds;
                ds["step"] = stepCount;
                ds["code"] = code;
                ds["output"] = entry;
                ds["newDictEntry"] = prevString + entry[0];
                ds["newDictIndex"] = dictSize;
                decodeSteps.push_back(std::move(ds));
            }
            dictSize++;
        }

        prevString = entry;
        stepCount++;
    }

    result.steps["decodeSteps"] = std::move(decodeSteps);
    result.steps["decompressedSize"] = (int)result.decompressedData.size();

    return result;
}
