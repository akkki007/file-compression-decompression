#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "crow.h"

static const int LZW_MAX_DICT_SIZE = 65536;
static const int LZW_INITIAL_BIT_SIZE = 9; // start at 9 bits (codes 0-511)
static const int LZW_MAX_BIT_SIZE = 16;    // matches LZW_MAX_DICT_SIZE = 2^16

struct LzwResult {
    std::vector<uint8_t> compressedData;
    crow::json::wvalue steps;
};

struct LzwDecompressResult {
    std::vector<uint8_t> decompressedData;
    crow::json::wvalue steps;
};

// Calculate minimum bits needed to represent values 0..maxVal
inline int bitsNeeded(int maxVal) {
    if (maxVal <= 1) return 1;
    int bits = 0;
    while (maxVal > 0) { bits++; maxVal >>= 1; }
    return bits;
}

inline LzwResult lzwCompress(const std::vector<uint8_t>& input) {
    LzwResult result;

    if (input.empty()) {
        result.steps["error"] = "Empty input";
        return result;
    }

    // Initialize dictionary with single-byte entries
    std::unordered_map<std::string, int> dict;
    for (int i = 0; i < 256; i++) dict[std::string(1, (char)i)] = i;
    int dictSize = 256;

    // Step tracking for visualization
    std::vector<crow::json::wvalue> dictSteps;
    std::vector<crow::json::wvalue> encodeSteps;

    // Variable-width encoding: start at 9 bits, grow as dictionary grows
    int currentBitSize = LZW_INITIAL_BIT_SIZE;
    std::string bitBuffer;
    std::vector<int> codes;

    auto writeBits = [&](int code, int numBits) {
        for (int b = numBits - 1; b >= 0; b--)
            bitBuffer += ((code >> b) & 1) ? '1' : '0';
    };

    auto flushBytes = [&]() {
        while (bitBuffer.size() >= 8) {
            uint8_t byte = 0;
            for (int j = 0; j < 8; j++) byte = (byte << 1) | (bitBuffer[j] - '0');
            result.compressedData.push_back(byte);
            bitBuffer = bitBuffer.substr(8);
        }
    };

    std::string current(1, (char)input[0]);
    int stepCount = 0;

    for (size_t i = 1; i < input.size(); i++) {
        std::string next = current + (char)input[i];
        if (dict.count(next)) {
            current = next;
        } else {
            int code = dict[current];
            codes.push_back(code);
            writeBits(code, currentBitSize);
            flushBytes();

            // Track steps for visualization (limit to 80)
            if (stepCount < 80) {
                crow::json::wvalue es;
                es["step"] = stepCount;
                es["sequence"] = current;
                es["code"] = code;
                es["newEntry"] = next;
                es["newCode"] = dictSize;
                es["bitSize"] = currentBitSize;
                encodeSteps.push_back(std::move(es));

                if (dictSize < LZW_MAX_DICT_SIZE) {
                    crow::json::wvalue ds;
                    ds["index"] = dictSize;
                    ds["entry"] = next;
                    dictSteps.push_back(std::move(ds));
                }
            }

            if (dictSize < LZW_MAX_DICT_SIZE) {
                dict[next] = dictSize++;
                // Grow bit size when dictionary exceeds current capacity.
                // Cap at LZW_MAX_BIT_SIZE so we never emit codes wider than
                // the dictionary can address.
                if (dictSize > (1 << currentBitSize) && currentBitSize < LZW_MAX_BIT_SIZE) {
                    currentBitSize++;
                }
            }
            current = std::string(1, (char)input[i]);
            stepCount++;
        }
    }

    // Output last code
    if (!current.empty()) {
        int code = dict[current];
        codes.push_back(code);
        writeBits(code, currentBitSize);
        flushBytes();
    }

    // Flush remaining bits with padding
    if (!bitBuffer.empty()) {
        while (bitBuffer.size() < 8) bitBuffer += '0';
        uint8_t byte = 0;
        for (int j = 0; j < 8; j++) byte = (byte << 1) | (bitBuffer[j] - '0');
        result.compressedData.push_back(byte);
    }

    // Prepend header: totalCodes (4 bytes)
    // No need to store bitSize since decompressor grows it in lockstep
    std::vector<uint8_t> finalData;
    uint32_t totalCodes = codes.size();
    finalData.push_back((totalCodes >> 24) & 0xFF);
    finalData.push_back((totalCodes >> 16) & 0xFF);
    finalData.push_back((totalCodes >> 8) & 0xFF);
    finalData.push_back(totalCodes & 0xFF);
    // Append compressed data
    finalData.insert(finalData.end(), result.compressedData.begin(), result.compressedData.end());
    result.compressedData = finalData;

    result.steps["encodeSteps"] = std::move(encodeSteps);
    result.steps["dictionarySteps"] = std::move(dictSteps);
    result.steps["bitSize"] = currentBitSize;
    result.steps["finalDictSize"] = dictSize;
    result.steps["totalCodes"] = (int)totalCodes;
    result.steps["originalSize"] = (int)input.size();
    result.steps["compressedSize"] = (int)result.compressedData.size();

    return result;
}

inline LzwDecompressResult lzwDecompress(const std::vector<uint8_t>& data) {
    LzwDecompressResult result;

    if (data.size() < 4) {
        result.steps["error"] = "Invalid compressed data";
        return result;
    }

    size_t pos = 0;

    // Read totalCodes
    uint32_t totalCodes = ((uint32_t)data[pos] << 24) | ((uint32_t)data[pos+1] << 16) |
                          ((uint32_t)data[pos+2] << 8) | data[pos+3];
    pos += 4;

    result.steps["totalCodes"] = (int)totalCodes;

    if (totalCodes == 0) {
        result.steps["error"] = "No codes to decompress";
        return result;
    }

    // Convert remaining bytes to bit string
    std::string bitString;
    for (size_t i = pos; i < data.size(); i++) {
        for (int j = 7; j >= 0; j--) bitString += ((data[i] >> j) & 1) ? '1' : '0';
    }

    // Initialize decompression dictionary
    std::unordered_map<int, std::string> dict;
    for (int i = 0; i < 256; i++) dict[i] = std::string(1, (char)i);
    int dictSize = 256;
    int currentBitSize = LZW_INITIAL_BIT_SIZE;

    std::vector<crow::json::wvalue> decodeSteps;

    // Read codes from bit string with variable width
    size_t bitPos = 0;
    auto readCode = [&]() -> int {
        if (bitPos + currentBitSize > bitString.size()) return -1;
        int code = 0;
        for (int i = 0; i < currentBitSize; i++)
            code = (code << 1) | (bitString[bitPos + i] - '0');
        bitPos += currentBitSize;
        return code;
    };

    int prevCode = readCode();
    if (prevCode < 0 || !dict.count(prevCode)) {
        result.steps["error"] = "Invalid first code";
        return result;
    }

    std::string prevString = dict[prevCode];
    for (char c : prevString) result.decompressedData.push_back((uint8_t)c);

    int stepCount = 0;
    for (uint32_t i = 1; i < totalCodes; i++) {
        int code = readCode();
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
                ds["bitSize"] = currentBitSize;
                decodeSteps.push_back(std::move(ds));
            }
            dictSize++;
            // Grow bit size: decompressor uses >= because its dict is one entry
            // behind the compressor (compressor adds entry right after outputting
            // a code, decompressor adds it while processing the next code).
            // The LZW_MAX_BIT_SIZE cap is required at the dict-full boundary:
            // the compressor's outer `dictSize < LZW_MAX_DICT_SIZE` guard
            // short-circuits before its `>` check fires when dictSize reaches
            // 65536, but the decompressor's guard still passes for that one
            // transition iteration and `>=` would otherwise bump bs to 17.
            if (dictSize >= (1 << currentBitSize) && currentBitSize < LZW_MAX_BIT_SIZE) {
                currentBitSize++;
            }
        }

        prevString = entry;
        stepCount++;
    }

    result.steps["decodeSteps"] = std::move(decodeSteps);
    result.steps["decompressedSize"] = (int)result.decompressedData.size();
    result.steps["bitSize"] = currentBitSize;

    return result;
}
