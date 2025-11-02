#include "hmicx.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstring>

using namespace std;
using namespace HMICX;

// 🔥 ALL INLINE HELPER FUNCTIONS NOW LIVE IN THE HEADER FILE 🔥
// (fastStartsWith, fastExtractNumber, findMatchingBrace, fastTrim)
// This prevents duplicate definitions and lets the compiler optimize better!!

Parser::Parser(const string& filepath) {
    cout << "[DEBUG] 🔥 Parser constructor called with: " << filepath << endl;
    
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
    
    cout << "[DEBUG] 📄 File loaded successfully! Size: " << size << " bytes" << endl;
}

void Parser::parse() {
    cout << "[DEBUG] 🚀 Starting parse()..." << endl;
    parseHeader();
    parseFrames();
    cout << "[DEBUG] ✅ Parsing complete!" << endl;
}

void Parser::parseHeader() {
    cout << "[DEBUG] 📋 Starting parseHeader()..." << endl;
    
    const char* data = content.c_str();
    size_t len = content.size();
    
    for (size_t pos = 0; pos < len - 4; pos++) {
        if (fastStartsWith(data + pos, len - pos, "info", 4)) {
            cout << "[DEBUG] 🔍 Found 'info' at position " << pos << endl;
            
            pos += 4;
            while (pos < len && isspace(data[pos])) pos++;
            
            if (pos < len && data[pos] == '{') {
                cout << "[DEBUG] 📦 Found opening brace for info block at pos " << pos << endl;
                
                size_t end = findMatchingBrace(data, len, pos);
                if (end != string::npos) {
                    cout << "[DEBUG] 📦 Found closing brace at pos " << end << endl;
                    parseHeaderBody(data + pos + 1, end - pos - 1);
                    cout << "[DEBUG] ✅ Header parsed successfully!" << endl;
                    return;
                }
            }
        }
    }
    
    cout << "[DEBUG] ⚠️ No header found in file!" << endl;
}

void Parser::parseHeaderBody(const char* body, size_t len) {
    cout << "[DEBUG] 📝 Parsing header body (length: " << len << ")" << endl;
    
    size_t lineStart = 0;
    int lines_parsed = 0;
    
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
                        string val(valPtr, valLen);
                        header[key] = val;
                        lines_parsed++;
                        cout << "[DEBUG]   📌 " << key << " = " << val << endl;
                    }
                }
            }
            lineStart = i + 1;
        }
    }
    
    cout << "[DEBUG] 📊 Parsed " << lines_parsed << " header lines" << endl;
}

void Parser::parseFrames() {
    cout << "[DEBUG] 🎬 Starting parseFrames()..." << endl;
    
    const char* data = content.c_str();
    size_t len = content.size();
    
    cout << "[DEBUG] 📄 File length: " << len << " bytes" << endl;
    cout << "[DEBUG] 📄 First 500 chars of file:" << endl;
    cout << "=================" << endl;
    cout << string(data, min(len, (size_t)500)) << endl;
    cout << "=================" << endl;
    
    // ⚡ Reserve space to avoid reallocation
    commands.reserve(1000);
    
    int frames_found = 0;
    
    for (size_t pos = 0; pos < len - 1; pos++) {
        if ((data[pos] == 'F' || data[pos] == 'f') && isdigit(data[pos + 1])) {
            cout << "[DEBUG] 🎯 Found 'F' followed by digit at position " << pos << endl;
            cout << "[DEBUG]   Context: '" << string(data + pos, min((size_t)20, len - pos)) << "'" << endl;
            
            pos++;
            int start = fastExtractNumber(data, len, pos);
            int end = start;
            
            if (pos < len && data[pos] == '-') {
                pos++;
                end = fastExtractNumber(data, len, pos);
            }
            
            cout << "[DEBUG] 📍 Frame range: " << start << "-" << end << endl;
            
            // Skip whitespace to find opening brace
            while (pos < len && isspace(data[pos])) pos++;
            
            if (pos >= len) {
                cout << "[DEBUG] ❌ Reached end of file before finding opening brace!" << endl;
                continue;
            }
            
            if (data[pos] != '{') {
                cout << "[DEBUG] ⚠️ No opening brace found! Next char: '" << data[pos] << "' (ASCII " << (int)data[pos] << ")" << endl;
                continue;
            }
            
            cout << "[DEBUG] 📦 Frame opening brace at pos " << pos << endl;
            
            size_t frameEnd = findMatchingBrace(data, len, pos);
            
            if (frameEnd == string::npos) {
                cout << "[DEBUG] ❌ No matching closing brace for frame!" << endl;
                continue;
            }
            
            size_t frameBodyLen = frameEnd - pos - 1;
            cout << "[DEBUG] 📦 Frame closing brace at pos " << frameEnd << " (body length: " << frameBodyLen << ")" << endl;
            
            if (frameBodyLen > 0) {
                cout << "[DEBUG] 📄 Frame body preview (first 200 chars):" << endl;
                cout << "~~~~~~~~~~~~~~~~~" << endl;
                cout << string(data + pos + 1, min((size_t)200, frameBodyLen)) << endl;
                cout << "~~~~~~~~~~~~~~~~~" << endl;
                
                parseFrameBody(data + pos + 1, frameBodyLen, start, end);
            } else {
                cout << "[DEBUG] ⚠️ Frame body is empty!" << endl;
            }
            
            pos = frameEnd;
            frames_found++;
        }
    }
    
    cout << "[DEBUG] 🎬 Total frames found: " << frames_found << endl;
    cout << "[DEBUG] 📊 Total commands: " << commands.size() << endl;
}

void Parser::parseFrameBody(const char* body, size_t len, int start, int end) {
    cout << "[DEBUG] 🔍 parseFrameBody called! Frame " << start << "-" << end << ", body length: " << len << endl;
    
    size_t pos = 0;
    int colors_found = 0;
    int commands_before = commands.size();
    
    while (pos < len) {
        string color;
        
        // 🎨 Check for rgba(...)
        if (pos + 5 <= len && fastStartsWith(body + pos, len - pos, "rgba(", 5)) {
            cout << "[DEBUG]   🎨 Found 'rgba(' at pos " << pos << endl;
            
            size_t parenEnd = pos + 5;
            while (parenEnd < len && body[parenEnd] != ')') parenEnd++;
            
            if (parenEnd < len && body[parenEnd] == ')') {
                color.assign(body + pos, parenEnd - pos + 1);
                cout << "[DEBUG]   ✅ Extracted RGBA color: " << color << endl;
                pos = parenEnd + 1;
                colors_found++;
            } else {
                cout << "[DEBUG]   ❌ RGBA has no closing paren! parenEnd=" << parenEnd << ", len=" << len << endl;
                pos++;
                continue;
            }
        }
        // 🎨 Check for rgb(...)
        else if (pos + 4 <= len && fastStartsWith(body + pos, len - pos, "rgb(", 4)) {
            cout << "[DEBUG]   🎨 Found 'rgb(' at pos " << pos << endl;
            
            size_t parenEnd = pos + 4;
            while (parenEnd < len && body[parenEnd] != ')') parenEnd++;
            
            if (parenEnd < len && body[parenEnd] == ')') {
                color.assign(body + pos, parenEnd - pos + 1);
                cout << "[DEBUG]   ✅ Extracted RGB color: " << color << endl;
                pos = parenEnd + 1;
                colors_found++;
            } else {
                cout << "[DEBUG]   ❌ RGB has no closing paren!" << endl;
                pos++;
                continue;
            }
        }
        // Check for hex color
        else if (pos < len && body[pos] == '#' && pos + 7 <= len) {
            bool isHex = true;
            for (int i = 1; i <= 6; i++) {
                if (!isxdigit(body[pos + i])) {
                    isHex = false;
                    break;
                }
            }
            if (isHex) {
                color.assign(body + pos, 7);
                cout << "[DEBUG]   ✅ Extracted HEX color: " << color << endl;
                pos += 7;
                colors_found++;
            } else {
                pos++;
                continue;
            }
        }
        else {
            // No color found at this position, move forward
            pos++;
            continue;
        }
        
        // At this point, we MUST have a non-empty color
        if (color.empty()) {
            cout << "[DEBUG]   ❌ Color is empty after detection?!" << endl;
            continue;
        }
        
        // Skip whitespace to find opening brace
        cout << "[DEBUG]   🔍 Looking for opening brace after color..." << endl;
        while (pos < len && isspace(body[pos])) pos++;
        
        if (pos >= len) {
            cout << "[DEBUG]   ❌ Reached end of body!" << endl;
            break;
        }
        
        if (body[pos] != '{') {
            cout << "[DEBUG]   ⚠️ No opening brace found for color " << color << "! Next char: '" << body[pos] << "' (ASCII " << (int)body[pos] << ")" << endl;
            continue;
        }
        
        cout << "[DEBUG]   📦 Found opening brace at pos " << pos << endl;
        
        // Find matching closing brace
        size_t blockEnd = findMatchingBrace(body, len, pos);
        
        if (blockEnd == string::npos) {
            cout << "[DEBUG]   ❌ No matching closing brace!" << endl;
            break;
        }
        
        size_t pixelBodyLen = blockEnd - pos - 1;
        cout << "[DEBUG]   📦 Found closing brace at pos " << blockEnd << " (pixel body length: " << pixelBodyLen << ")" << endl;
        
        if (pixelBodyLen > 0) {
            cout << "[DEBUG]   📄 Pixel body preview (first 100 chars): " << string(body + pos + 1, min((size_t)100, pixelBodyLen)) << endl;
        }
        
        // Parse pixels inside the block
        vector<Pixel> pixels = parsePixels(body + pos + 1, pixelBodyLen);
        
        cout << "[DEBUG]   💎 Parsed " << pixels.size() << " pixels for color " << color << endl;
        
        if (!pixels.empty()) {
            commands.push_back({start, end, std::move(pixels), std::move(color)});
            cout << "[DEBUG]   ✅ Added command with " << pixels.size() << " pixels" << endl;
        } else {
            cout << "[DEBUG]   ⚠️ No pixels found in color block!" << endl;
        }
        
        // Move past the closing brace
        pos = blockEnd + 1;
    }
    
    int commands_added = commands.size() - commands_before;
    cout << "[DEBUG] 🎨 Frame summary: " << colors_found << " colors found, " << commands_added << " commands added" << endl;
}

vector<Pixel> Parser::parsePixels(const char* body, size_t len) {
    cout << "[DEBUG]     🔍 parsePixels called with length " << len << endl;
    
    vector<Pixel> pixels;
    pixels.reserve(100);
    
    size_t lineStart = 0;
    int lines_processed = 0;
    int p_commands = 0;
    int pl_commands = 0;
    
    for (size_t i = 0; i <= len; i++) {
        if (i == len || body[i] == '\n') {
            // Trim line inline
            size_t start = lineStart;
            size_t end = i;
            
            while (start < end && isspace(body[start])) start++;
            while (start < end && isspace(body[end - 1])) end--;
            
            if (end > start) {
                lines_processed++;
                
                // Check prefix case-insensitively
                if (end - start > 2 && (body[start] == 'p' || body[start] == 'P') && body[start + 1] == '=') {
                    // Parse P=1x2,3x4
                    p_commands++;
                    size_t pos = start + 2;
                    int pixels_this_line = 0;
                    
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
                            pixels_this_line++;
                        }
                        
                        if (pos < end && body[pos] == ',') pos++;
                        while (pos < end && isspace(body[pos])) pos++;
                    }
                    
                    if (lines_processed <= 3) {
                        cout << "[DEBUG]       📌 P command: '" << string(body + start, end - start) << "' → " << pixels_this_line << " pixels" << endl;
                    }
                }
                else if (end - start > 3 && (body[start] == 'p' || body[start] == 'P') && 
                         (body[start + 1] == 'l' || body[start + 1] == 'L') && body[start + 2] == '=') {
                    // Parse PL=1x1-10x1
                    pl_commands++;
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
                    
                    int pixels_before = pixels.size();
                    
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
                    
                    int pixels_added = pixels.size() - pixels_before;
                    
                    if (pl_commands <= 3) {
                        cout << "[DEBUG]       📌 PL command: '" << string(body + start, end - start) << "' → " << pixels_added << " pixels" << endl;
                    }
                }
            }
            
            lineStart = i + 1;
        }
    }
    
    cout << "[DEBUG]     📊 Pixel parsing summary: " << lines_processed << " lines, " << p_commands << " P commands, " << pl_commands << " PL commands → " << pixels.size() << " total pixels" << endl;
    
    return pixels;
}

map<string, string> Parser::getHeader() const {
    return header;
}

vector<Command> Parser::getCommands() const {
    return commands;
}