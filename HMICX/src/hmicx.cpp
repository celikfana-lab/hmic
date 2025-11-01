#include "hmicx.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstring>

using namespace std;
using namespace HMICX;

// ⚡ OPTIMIZED: No substring creation
inline bool fastStartsWith(const char* str, size_t len, const char* prefix, size_t prefixLen) {
    if (len < prefixLen) return false;
    for (size_t i = 0; i < prefixLen; i++) {
        if (tolower(str[i]) != tolower(prefix[i])) return false;
    }
    return true;
}

// ⚡ OPTIMIZED: Parse number without string creation
inline int fastExtractNumber(const char* str, size_t len, size_t& pos) {
    int num = 0;
    bool found = false;
    while (pos < len && isdigit(str[pos])) {
        num = num * 10 + (str[pos] - '0');
        pos++;
        found = true;
    }
    return found ? num : -1;
}

// ⚡ OPTIMIZED: Find brace scanning once
size_t findMatchingBrace(const char* s, size_t len, size_t start) {
    int depth = 0;
    for (size_t i = start; i < len; i++) {
        if (s[i] == '{') depth++;
        else if (s[i] == '}') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return string::npos;
}

// ⚡ OPTIMIZED: Trim using pointers
inline pair<const char*, size_t> fastTrim(const char* start, size_t len) {
    const char* end = start + len;
    while (start < end && isspace(*start)) start++;
    while (start < end && isspace(*(end - 1))) end--;
    return {start, static_cast<size_t>(end - start)};
}

Parser::Parser(const string& filepath) {
    ifstream f(filepath, ios::binary | ios::ate);
    if (!f.is_open()) throw runtime_error("Cannot open file: " + filepath);
    
    // ⚡ Read entire file at once with known size
    streamsize size = f.tellg();
    f.seekg(0, ios::beg);
    content.resize(size);
    if (!f.read(&content[0], size)) {
        throw runtime_error("Failed to read file: " + filepath);
    }
    f.close();
}

void Parser::parse() {
    parseHeader();
    parseFrames();
}

void Parser::parseHeader() {
    const char* data = content.c_str();
    size_t len = content.size();
    
    for (size_t pos = 0; pos < len - 4; pos++) {
        if (fastStartsWith(data + pos, len - pos, "info", 4)) {
            pos += 4;
            while (pos < len && isspace(data[pos])) pos++;
            
            if (pos < len && data[pos] == '{') {
                size_t end = findMatchingBrace(data, len, pos);
                if (end != string::npos) {
                    parseHeaderBody(data + pos + 1, end - pos - 1);
                    return;
                }
            }
        }
    }
}

void Parser::parseHeaderBody(const char* body, size_t len) {
    size_t lineStart = 0;
    
    for (size_t i = 0; i <= len; i++) {
        if (i == len || body[i] == '\n') {
            auto [linePtr, lineLen] = fastTrim(body + lineStart, i - lineStart);
            
            if (lineLen > 0) {
                // Find '=' without creating substring
                const char* eq = (const char*)memchr(linePtr, '=', lineLen);
                if (eq) {
                    size_t eqPos = eq - linePtr;
                    auto [keyPtr, keyLen] = fastTrim(linePtr, eqPos);
                    auto [valPtr, valLen] = fastTrim(eq + 1, lineLen - eqPos - 1);
                    
                    if (keyLen > 0 && valLen > 0) {
                        string key(keyPtr, keyLen);
                        transform(key.begin(), key.end(), key.begin(), ::toupper);
                        header[key] = string(valPtr, valLen);
                    }
                }
            }
            lineStart = i + 1;
        }
    }
}

void Parser::parseFrames() {
    const char* data = content.c_str();
    size_t len = content.size();
    
    // ⚡ Reserve space to avoid reallocation
    commands.reserve(1000);
    
    for (size_t pos = 0; pos < len - 1; pos++) {
        if ((data[pos] == 'F' || data[pos] == 'f') && isdigit(data[pos + 1])) {
            pos++;
            int start = fastExtractNumber(data, len, pos);
            int end = start;
            
            if (pos < len && data[pos] == '-') {
                pos++;
                end = fastExtractNumber(data, len, pos);
            }
            
            while (pos < len && data[pos] != '{') pos++;
            
            if (pos < len) {
                size_t frameEnd = findMatchingBrace(data, len, pos);
                if (frameEnd != string::npos) {
                    parseFrameBody(data + pos + 1, frameEnd - pos - 1, start, end);
                    pos = frameEnd;
                }
            }
        }
    }
}

void Parser::parseFrameBody(const char* body, size_t len, int start, int end) {
    size_t pos = 0;
    
    while (pos < len) {
        string color;
        
        // Check for rgb(...)
        if (pos + 4 <= len && fastStartsWith(body + pos, len - pos, "rgb(", 4)) {
            size_t parenEnd = pos + 4;
            while (parenEnd < len && body[parenEnd] != ')') parenEnd++;
            if (parenEnd < len) {
                color.assign(body + pos, parenEnd - pos + 1);
                pos = parenEnd + 1;
            }
        }
        // Check for hex color
        else if (body[pos] == '#' && pos + 7 <= len) {
            bool isHex = true;
            for (int i = 1; i <= 6; i++) {
                if (!isxdigit(body[pos + i])) {
                    isHex = false;
                    break;
                }
            }
            if (isHex) {
                color.assign(body + pos, 7);
                pos += 7;
            }
        }
        
        if (!color.empty()) {
            while (pos < len && body[pos] != '{') pos++;
            
            if (pos < len) {
                size_t blockEnd = findMatchingBrace(body, len, pos);
                if (blockEnd != string::npos) {
                    vector<Pixel> pixels = parsePixels(body + pos + 1, blockEnd - pos - 1);
                    
                    if (!pixels.empty()) {
                        commands.push_back({start, end, std::move(pixels), std::move(color)});
                    }
                    
                    pos = blockEnd + 1;
                    continue;
                }
            }
        }
        
        pos++;
    }
}

vector<Pixel> Parser::parsePixels(const char* body, size_t len) {
    vector<Pixel> pixels;
    pixels.reserve(100); // ⚡ Pre-allocate
    
    size_t lineStart = 0;
    
    for (size_t i = 0; i <= len; i++) {
        if (i == len || body[i] == '\n') {
            // Trim line inline
            size_t start = lineStart;
            size_t end = i;
            
            while (start < end && isspace(body[start])) start++;
            while (start < end && isspace(body[end - 1])) end--;
            
            if (end > start) {
                // Check prefix case-insensitively
                if (end - start > 2 && (body[start] == 'p' || body[start] == 'P') && body[start + 1] == '=') {
                    // Parse P=1x2,3x4
                    size_t pos = start + 2;
                    while (pos < end) {
                        int x = 0, y = 0;
                        bool hasX = false, hasY = false;
                        
                        // Parse x
                        while (pos < end && isdigit(body[pos])) {
                            x = x * 10 + (body[pos] - '0');
                            pos++;
                            hasX = true;
                        }
                        
                        if (pos < end && (body[pos] == 'x' || body[pos] == 'X')) {
                            pos++;
                            // Parse y
                            while (pos < end && isdigit(body[pos])) {
                                y = y * 10 + (body[pos] - '0');
                                pos++;
                                hasY = true;
                            }
                        }
                        
                        if (hasX && hasY) {
                            pixels.push_back({x, y});
                        }
                        
                        if (pos < end && body[pos] == ',') pos++;
                        while (pos < end && isspace(body[pos])) pos++;
                    }
                }
                else if (end - start > 3 && (body[start] == 'p' || body[start] == 'P') && 
                         (body[start + 1] == 'l' || body[start + 1] == 'L') && body[start + 2] == '=') {
                    // Parse PL=1x1-10x1
                    size_t pos = start + 3;
                    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                    int* current = &x1;
                    
                    while (pos < end) {
                        if (isdigit(body[pos])) {
                            *current = *current * 10 + (body[pos] - '0');
                        } else if (body[pos] == 'x' || body[pos] == 'X') {
                            if (current == &x1) current = &y1;
                            else if (current == &x2) current = &y2;
                        } else if (body[pos] == '-') {
                            current = &x2;
                        }
                        pos++;
                    }
                    
                    // Generate line
                    if (y1 == y2) {
                        int minX = min(x1, x2);
                        int maxX = max(x1, x2);
                        for (int x = minX; x <= maxX; x++) {
                            pixels.push_back({x, y1});
                        }
                    } else if (x1 == x2) {
                        int minY = min(y1, y2);
                        int maxY = max(y1, y2);
                        for (int y = minY; y <= maxY; y++) {
                            pixels.push_back({x1, y});
                        }
                    }
                }
            }
            
            lineStart = i + 1;
        }
    }
    
    return pixels;
}

map<string, string> Parser::getHeader() const {
    return header;
}

vector<Command> Parser::getCommands() const {
    return commands;
}