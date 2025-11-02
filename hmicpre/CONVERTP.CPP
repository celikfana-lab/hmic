#include "hmicx.h"
#include <zstd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cstring>
#include <algorithm>
#include <cstdint>  // 🔥 THIS WAS MISSING - ABSOLUTELY CRUCIAL!!

using namespace std;
using namespace HMICX;

// 🎨 HMICP FORMAT STRUCTURE (Binary Blob Edition) 🎨
// 
// HEADER (fixed size):
// - Magic: "HMICP" (5 bytes)
// - Version: uint8_t (1 byte)
// - Width: uint16_t (2 bytes)
// - Height: uint16_t (2 bytes)
// - FPS: uint16_t (2 bytes)
// - Total Frames: uint32_t (4 bytes)
// - Loop: uint8_t (1 byte, 0=no, 1=yes)
// - Reserved: 7 bytes (for future use)
// Total: 24 bytes
//
// FRAME DATA (per frame):
// - Each pixel is 4 bytes: RGBA
// - Frame size = Width * Height * 4 bytes
// - All frames stored sequentially

struct HMICPHeader {
    char magic[5] = {'H', 'M', 'I', 'C', 'P'};
    uint8_t version = 1;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t fps = 0;
    uint32_t totalFrames = 0;
    uint8_t loop = 0;
    uint8_t reserved[7] = {0};
} __attribute__((packed));

struct RGBA {
    uint8_t r, g, b, a;  // 🎨 NOW WITH PROPER TYPES!!
};

// 🔥 Parse color string to RGBA
RGBA parseColor(const string& s) {
    RGBA c = {255, 255, 255, 255};
    string str = s;
    transform(str.begin(), str.end(), str.begin(), ::tolower);

    if (str.rfind("#", 0) == 0 && str.size() == 7) {
        int r = stoi(str.substr(1, 2), nullptr, 16);
        int g = stoi(str.substr(3, 2), nullptr, 16);
        int b = stoi(str.substr(5, 2), nullptr, 16);
        c.r = (uint8_t)r;
        c.g = (uint8_t)g;
        c.b = (uint8_t)b;
        c.a = 255;
    } 
    else if (str.find("rgba(") == 0) {
        int r, g, b, a;
        if (sscanf(str.c_str(), "rgba(%d,%d,%d,%d)", &r, &g, &b, &a) == 4) {
            c.r = (uint8_t)r;
            c.g = (uint8_t)g;
            c.b = (uint8_t)b;
            c.a = (uint8_t)a;
        }
    }
    else if (str.find("rgb(") == 0) {
        int r, g, b;
        if (sscanf(str.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3) {
            c.r = (uint8_t)r;
            c.g = (uint8_t)g;
            c.b = (uint8_t)b;
            c.a = 255;
        }
    }
    
    return c;
}

// 🎬 Render all frames into memory
vector<vector<RGBA>> renderAllFrames(const vector<Command>& commands, int width, int height, int totalFrames) {
    cout << "[DEBUG] 🎬 Pre-rendering " << totalFrames << " frames..." << endl;
    
    vector<vector<RGBA>> frames(totalFrames);
    
    // Initialize all frames with black transparent background
    RGBA blackTransparent = {0, 0, 0, 0};
    for (int f = 0; f < totalFrames; f++) {
        frames[f].resize(width * height, blackTransparent);
    }
    
    // Apply commands
    int commandsProcessed = 0;
    for (const auto& cmd : commands) {
        RGBA color = parseColor(cmd.color);
        
        // Apply to all frames in range
        for (int f = cmd.start; f <= cmd.end && f <= totalFrames; f++) {
            int frameIdx = f - 1; // Convert to 0-based
            if (frameIdx < 0 || frameIdx >= totalFrames) continue;
            
            for (const auto& px : cmd.pixels) {
                int x = px.x - 1; // Convert to 0-based
                int y = px.y - 1;
                
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    int pixelIdx = y * width + x;
                    
                    // Alpha blending (if background isn't fully transparent)
                    RGBA& bg = frames[frameIdx][pixelIdx];
                    if (color.a == 255) {
                        // Opaque - just replace
                        bg = color;
                    } else if (color.a > 0) {
                        // Blend
                        float alpha = color.a / 255.0f;
                        float invAlpha = 1.0f - alpha;
                        bg.r = (uint8_t)(color.r * alpha + bg.r * invAlpha);
                        bg.g = (uint8_t)(color.g * alpha + bg.g * invAlpha);
                        bg.b = (uint8_t)(color.b * alpha + bg.b * invAlpha);
                        bg.a = max(bg.a, color.a); // Keep max alpha
                    }
                }
            }
        }
        
        commandsProcessed++;
        if (commandsProcessed % 100 == 0 || commandsProcessed == commands.size()) {
            cout << "[DEBUG]   📊 Processed " << commandsProcessed << "/" << commands.size() << " commands" << endl;
        }
    }
    
    cout << "[DEBUG] ✅ Pre-rendering complete!" << endl;
    return frames;
}

// 💾 Write HMICP file
void writeHMICP(const string& outputPath, const HMICPHeader& header, const vector<vector<RGBA>>& frames) {
    cout << "[DEBUG] 💾 Writing HMICP to: " << outputPath << endl;
    
    ofstream out(outputPath, ios::binary);
    if (!out.is_open()) {
        throw runtime_error("Failed to create output file: " + outputPath);
    }
    
    // Write header
    out.write(reinterpret_cast<const char*>(&header), sizeof(HMICPHeader));
    
    // Write frame data
    size_t bytesWritten = 0;
    for (const auto& frame : frames) {
        out.write(reinterpret_cast<const char*>(frame.data()), frame.size() * sizeof(RGBA));
        bytesWritten += frame.size() * sizeof(RGBA);
    }
    
    out.close();
    
    cout << "[DEBUG] ✅ Wrote " << bytesWritten << " bytes of frame data" << endl;
    cout << "[DEBUG] 📦 Total file size: " << (sizeof(HMICPHeader) + bytesWritten) << " bytes" << endl;
}

// 🗜️ Compress HMICP to HMICP7 (Zstd compressed version)
void compressToHMICP7(const string& hmicpPath, const string& hmicp7Path) {
    cout << "[DEBUG] 🗜️ Compressing to HMICP7..." << endl;
    
    // Read HMICP file
    ifstream in(hmicpPath, ios::binary | ios::ate);
    if (!in.is_open()) {
        throw runtime_error("Failed to open HMICP file: " + hmicpPath);
    }
    
    streamsize size = in.tellg();
    in.seekg(0, ios::beg);
    vector<char> buffer(size);
    
    if (!in.read(buffer.data(), size)) {
        throw runtime_error("Failed to read HMICP file");
    }
    in.close();
    
    cout << "[DEBUG] 📄 Read " << size << " bytes from HMICP" << endl;
    
    // Compress with Zstd (level 19 = maximum compression)
    size_t compressBound = ZSTD_compressBound(size);
    vector<char> compressed(compressBound);
    
    size_t compressedSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        buffer.data(), buffer.size(),
        19 // Max compression level for MAXIMUM CHONK REDUCTION
    );
    
    if (ZSTD_isError(compressedSize)) {
        throw runtime_error(string("Zstd compression failed: ") + ZSTD_getErrorName(compressedSize));
    }
    
    compressed.resize(compressedSize);
    
    // Write compressed file
    ofstream out(hmicp7Path, ios::binary);
    if (!out.is_open()) {
        throw runtime_error("Failed to create HMICP7 file: " + hmicp7Path);
    }
    
    out.write(compressed.data(), compressed.size());
    out.close();
    
    float ratio = (1.0f - (float)compressedSize / (float)size) * 100.0f;
    cout << "[DEBUG] 🔥 Compressed " << size << " → " << compressedSize 
         << " bytes (" << ratio << "% reduction) SHEEEESH!! 💪" << endl;
}

int main() {
    cout << "🔥🔥🔥 HMIC → HMICP CONVERTER (BLOB MODE ACTIVATED) 🔥🔥🔥" << endl;
    cout << "This converter goes ABSOLUTELY CRAZY ngl fr fr!! 💯" << endl;
    cout << endl;
    
    string inputPath;
    cout << "Enter HMIC/HMIC7 file path: ";
    getline(cin, inputPath);
    
    try {
        // Determine if input is compressed
        bool isCompressed = false;
        if (inputPath.size() >= 6) {
            string ext = inputPath.substr(inputPath.size() - 6);
            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            isCompressed = (ext == ".hmic7");
        }
        
        string parsePath = inputPath;
        string tempFile = ".temp_decompressed.hmic";
        
        // Decompress if needed
        if (isCompressed) {
            cout << "[DEBUG] 🌀 Detected HMIC7 - decompressing..." << endl;
            
            ifstream in(inputPath, ios::binary | ios::ate);
            if (!in.is_open()) {
                throw runtime_error("Failed to open HMIC7 file");
            }
            
            streamsize size = in.tellg();
            in.seekg(0, ios::beg);
            vector<char> compressed(size);
            
            if (!in.read(compressed.data(), size)) {
                throw runtime_error("Failed to read HMIC7 file");
            }
            in.close();
            
            unsigned long long decompSize = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
            vector<char> decompressed(decompSize);
            
            size_t result = ZSTD_decompress(
                decompressed.data(), decompressed.size(),
                compressed.data(), compressed.size()
            );
            
            if (ZSTD_isError(result)) {
                throw runtime_error(string("Decompression failed: ") + ZSTD_getErrorName(result));
            }
            
            ofstream temp(tempFile);
            temp.write(decompressed.data(), decompressed.size());
            temp.close();
            
            parsePath = tempFile;
            cout << "[DEBUG] ✅ Decompressed successfully!" << endl;
        }
        
        // Parse HMIC
        cout << "[DEBUG] 📖 Parsing HMIC file..." << endl;
        Parser parser(parsePath);
        parser.parse();
        
        auto header = parser.getHeader();
        auto commands = parser.getCommands();
        
        // Extract metadata
        int width = 5, height = 5, fps = 2, totalFrames = 1;
        bool loop = true;
        
        for (const auto& [key, val] : header) {
            string k = key;
            transform(k.begin(), k.end(), k.begin(), ::toupper);
            
            if (k == "DISPLAY") {
                // Handle BOTH uppercase X and lowercase x in dimensions!!
                string v = val;
                transform(v.begin(), v.end(), v.begin(), ::tolower);
                if (sscanf(v.c_str(), "%dx%d", &width, &height) != 2) {
                    // Fallback: try original case
                    sscanf(val.c_str(), "%dx%d", &width, &height);
                }
                cout << "[DEBUG] 🎯 Parsed DISPLAY: " << width << "x" << height << " from '" << val << "'" << endl;
            } else if (k == "FPS") {
                fps = stoi(val);
            } else if (k == "F") {
                totalFrames = stoi(val);
            } else if (k == "LOOP") {
                loop = (val == "Y" || val == "y" || val == "1");
            }
        }
        
        cout << "[DEBUG] 📊 Metadata: " << width << "x" << height 
             << ", " << fps << " FPS, " << totalFrames << " frames, "
             << "Loop=" << (loop ? "YES" : "NO") << endl;
        
        // Create HMICP header
        HMICPHeader hmicpHeader;
        hmicpHeader.width = width;
        hmicpHeader.height = height;
        hmicpHeader.fps = fps;
        hmicpHeader.totalFrames = totalFrames;
        hmicpHeader.loop = loop ? 1 : 0;
        
        // Render all frames
        auto frames = renderAllFrames(commands, width, height, totalFrames);
        
        // Generate output paths
        string baseName = inputPath;
        if (isCompressed) {
            baseName = baseName.substr(0, baseName.size() - 6); // Remove .hmic7
        } else {
            baseName = baseName.substr(0, baseName.size() - 5); // Remove .hmic
        }
        
        string hmicpPath = baseName + ".hmicp";
        string hmicp7Path = baseName + ".hmicp7";
        
        // Write HMICP
        writeHMICP(hmicpPath, hmicpHeader, frames);
        cout << "✅ Created: " << hmicpPath << endl;
        
        // Compress to HMICP7
        compressToHMICP7(hmicpPath, hmicp7Path);
        cout << "✅ Created: " << hmicp7Path << endl;
        
        // Cleanup
        if (isCompressed) {
            remove(tempFile.c_str());
        }
        
        cout << endl;
        cout << "🎉🎉🎉 CONVERSION COMPLETE - THAT WAS BUSSIN FR FR!! 🎉🎉🎉" << endl;
        cout << "You now have:" << endl;
        cout << "  - " << hmicpPath << " (uncompressed blob - CHONKY)" << endl;
        cout << "  - " << hmicp7Path << " (compressed blob - SMOL)" << endl;
        cout << endl;
        cout << "These files are READY TO YEET into a renderer!! 🚀💪" << endl;
        
    } catch (const exception& e) {
        cerr << "❌ ERROR: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}