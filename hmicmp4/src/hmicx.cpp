#include "hmicx.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstring>

using namespace std;
using namespace HMICX;

// 🔥 STREAMING BUFFER SIZE - ADJUST THIS BASED ON YOUR VIBE
constexpr size_t BUFFER_SIZE = 8192; // 8KB chunks = chef's kiss 👨‍🍳

Parser::Parser(const string& filepath) {
    this->filepath = filepath;
    cout << "[DEBUG] 🔥 Streaming Parser constructor called with: " << filepath << endl;
    
    // Just check if file exists, don't load it yet!
    ifstream test(filepath);
    if (!test.is_open()) throw runtime_error("Cannot open file: " + filepath);
    test.close();
    
    cout << "[DEBUG] ✅ File exists and is readable! Ready to stream 🌊" << endl;
}

void Parser::parse() {
    cout << "[DEBUG] 🚀 Starting streaming parse()..." << endl;
    parseHeader();
    parseFrames();
    cout << "[DEBUG] ✅ Streaming parse complete!" << endl;
}

void Parser::parseHeader() {
    cout << "[DEBUG] 📋 Starting streaming parseHeader()..." << endl;
    
    ifstream file(filepath);
    if (!file.is_open()) throw runtime_error("Cannot open file");
    
    string buffer;
    buffer.reserve(BUFFER_SIZE);
    string accumulator; // For handling tokens that span chunks
    
    bool foundInfo = false;
    int braceDepth = 0;
    string headerContent;
    
    while (file) {
        buffer.resize(BUFFER_SIZE);
        file.read(&buffer[0], BUFFER_SIZE);
        streamsize bytesRead = file.gcount();
        buffer.resize(bytesRead);
        
        // Append to accumulator to handle spanning
        accumulator += buffer;
        
        // Look for "info {" block
        for (size_t i = 0; i < accumulator.size(); i++) {
            if (!foundInfo) {
                if (i + 4 <= accumulator.size() && 
                    fastStartsWith(accumulator.c_str() + i, accumulator.size() - i, "info", 4)) {
                    cout << "[DEBUG] 🔍 Found 'info' keyword!" << endl;
                    
                    // Skip to opening brace
                    size_t j = i + 4;
                    while (j < accumulator.size() && isspace(accumulator[j])) j++;
                    
                    if (j < accumulator.size() && accumulator[j] == '{') {
                        foundInfo = true;
                        braceDepth = 1;
                        i = j;
                        cout << "[DEBUG] 📦 Found opening brace!" << endl;
                        continue;
                    }
                }
            } else {
                // We're inside the info block, collect content
                if (accumulator[i] == '{') braceDepth++;
                else if (accumulator[i] == '}') {
                    braceDepth--;
                    if (braceDepth == 0) {
                        // Done! Parse the header content
                        cout << "[DEBUG] ✅ Found closing brace! Header length: " << headerContent.size() << endl;
                        parseHeaderBody(headerContent.c_str(), headerContent.size());
                        file.close();
                        return;
                    }
                }
                
                if (braceDepth > 0) {
                    headerContent += accumulator[i];
                }
            }
        }
        
        // Keep last bit in case token spans chunks
        if (accumulator.size() > 100 && !foundInfo) {
            accumulator = accumulator.substr(accumulator.size() - 100);
        }
    }
    
    file.close();
    cout << "[DEBUG] ⚠️ No complete header found in file!" << endl;
}

void Parser::parseHeaderBody(const char* body, size_t len) {
    cout << "[DEBUG] 📝 Parsing header body (length: " << len << ")" << endl;
    
    size_t lineStart = 0;
    int lines_parsed = 0;
    
    for (size_t i = 0; i <= len; i++) {
        if (i == len || body[i] == '\n') {
            auto [linePtr, lineLen] = fastTrim(body + lineStart, i - lineStart);
            
            if (lineLen > 0) {
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
    cout << "[DEBUG] 🎬 Starting streaming parseFrames()..." << endl;
    
    ifstream file(filepath);
    if (!file.is_open()) throw runtime_error("Cannot open file");
    
    commands.reserve(1000);
    
    string buffer;
    buffer.reserve(BUFFER_SIZE);
    string accumulator;
    
    int frames_found = 0;
    
    // State machine for frame parsing
    enum State { LOOKING_FOR_FRAME, IN_FRAME };
    State state = LOOKING_FOR_FRAME;
    
    int frameStart = 0, frameEnd = 0;
    int braceDepth = 0;
    string frameContent;
    
    while (file) {
        buffer.resize(BUFFER_SIZE);
        file.read(&buffer[0], BUFFER_SIZE);
        streamsize bytesRead = file.gcount();
        buffer.resize(bytesRead);
        
        accumulator += buffer;
        
        size_t processedUntil = 0;
        
        for (size_t i = 0; i < accumulator.size(); i++) {
            if (state == LOOKING_FOR_FRAME) {
                // Look for F1234 or F1-10 pattern
                if ((accumulator[i] == 'F' || accumulator[i] == 'f') && 
                    i + 1 < accumulator.size() && isdigit(accumulator[i + 1])) {
                    
                    cout << "[DEBUG] 🎯 Found frame marker at offset!" << endl;
                    
                    size_t pos = i + 1;
                    frameStart = fastExtractNumber(accumulator.c_str(), accumulator.size(), pos);
                    frameEnd = frameStart;
                    
                    if (pos < accumulator.size() && accumulator[pos] == '-') {
                        pos++;
                        frameEnd = fastExtractNumber(accumulator.c_str(), accumulator.size(), pos);
                    }
                    
                    cout << "[DEBUG] 📍 Frame range: " << frameStart << "-" << frameEnd << endl;
                    
                    // Skip to opening brace
                    while (pos < accumulator.size() && isspace(accumulator[pos])) pos++;
                    
                    if (pos < accumulator.size() && accumulator[pos] == '{') {
                        state = IN_FRAME;
                        braceDepth = 1;
                        i = pos;
                        frameContent.clear();
                        cout << "[DEBUG] 📦 Entering frame body!" << endl;
                    }
                }
            } else {
                // IN_FRAME state - collect frame content
                if (accumulator[i] == '{') braceDepth++;
                else if (accumulator[i] == '}') {
                    braceDepth--;
                    if (braceDepth == 0) {
                        // Frame complete!
                        cout << "[DEBUG] ✅ Frame " << frameStart << "-" << frameEnd << " complete! Size: " << frameContent.size() << endl;
                        
                        parseFrameBody(frameContent.c_str(), frameContent.size(), frameStart, frameEnd);
                        
                        state = LOOKING_FOR_FRAME;
                        frames_found++;
                        processedUntil = i + 1;
                        frameContent.clear();
                        continue;
                    }
                }
                
                if (braceDepth > 0) {
                    frameContent += accumulator[i];
                }
                processedUntil = i + 1;
            }
        }
        
        // Keep unprocessed data for next iteration
        if (state == LOOKING_FOR_FRAME) {
            // Keep last 100 chars in case frame marker spans chunks
            if (accumulator.size() > 100) {
                accumulator = accumulator.substr(accumulator.size() - 100);
            } else {
                accumulator.clear();
            }
        } else {
            // In frame - keep everything after processed
            if (processedUntil > 0) {
                accumulator = accumulator.substr(processedUntil);
            }
        }
    }
    
    file.close();
    
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
            pos++;
            continue;
        }
        
        if (color.empty()) {
            cout << "[DEBUG]   ❌ Color is empty after detection?!" << endl;
            continue;
        }
        
        // Skip whitespace to find opening brace
        while (pos < len && isspace(body[pos])) pos++;
        
        if (pos >= len) {
            cout << "[DEBUG]   ❌ Reached end of body!" << endl;
            break;
        }
        
        if (body[pos] != '{') {
            cout << "[DEBUG]   ⚠️ No opening brace found for color " << color << endl;
            continue;
        }
        
        // Find matching closing brace
        size_t blockEnd = findMatchingBrace(body, len, pos);
        
        if (blockEnd == string::npos) {
            cout << "[DEBUG]   ❌ No matching closing brace!" << endl;
            break;
        }
        
        size_t pixelBodyLen = blockEnd - pos - 1;
        
        // Parse pixels inside the block
        vector<Pixel> pixels = parsePixels(body + pos + 1, pixelBodyLen);
        
        cout << "[DEBUG]   💎 Parsed " << pixels.size() << " pixels for color " << color << endl;
        
        if (!pixels.empty()) {
            commands.push_back({start, end, std::move(pixels), std::move(color)});
            cout << "[DEBUG]   ✅ Added command with " << pixels.size() << " pixels" << endl;
        }
        
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
    int p_commands = 0;
    int pl_commands = 0;
    
    for (size_t i = 0; i <= len; i++) {
        if (i == len || body[i] == '\n') {
            size_t start = lineStart;
            size_t end = i;
            
            while (start < end && isspace(body[start])) start++;
            while (start < end && isspace(body[end - 1])) end--;
            
            if (end > start) {
                // Check for P= command
                if (end - start > 2 && (body[start] == 'p' || body[start] == 'P') && body[start + 1] == '=') {
                    p_commands++;
                    size_t pos = start + 2;
                    
                    while (pos < end) {
                        int x = 0, y = 0;
                        bool hasX = false, hasY = false;
                        
                        while (pos < end && isdigit(body[pos])) {
                            x = x * 10 + (body[pos] - '0');
                            pos++;
                            hasX = true;
                        }
                        
                        if (pos < end && (body[pos] == 'x' || body[pos] == 'X')) {
                            pos++;
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
                // Check for PL= command
                else if (end - start > 3 && (body[start] == 'p' || body[start] == 'P') && 
                         (body[start + 1] == 'l' || body[start + 1] == 'L') && body[start + 2] == '=') {
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
    
    cout << "[DEBUG]     📊 Parsed " << p_commands << " P commands, " << pl_commands << " PL commands → " << pixels.size() << " total pixels" << endl;
    
    return pixels;
}

map<string, string> Parser::getHeader() const {
    return header;
}

vector<Command> Parser::getCommands() const {
    return commands;
}