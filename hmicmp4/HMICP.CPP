#include <SDL2/SDL.h>
#include <zstd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>

using namespace std;

// 🎨 HMICP HEADER STRUCTURE (SAME AS CONVERTER)
struct HMICPHeader {
    char magic[5];
    uint8_t version;
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint32_t totalFrames;
    uint8_t loop;
    uint8_t reserved[7];
} __attribute__((packed));

struct RGBA {
    uint8_t r, g, b, a;
};

// 🔥 LOAD HMICP FILE (UNCOMPRESSED)
pair<HMICPHeader, vector<vector<RGBA>>> loadHMICP(const string& path) {
    cout << "[DEBUG] 📂 Loading HMICP file: " << path << endl;
    
    ifstream file(path, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Failed to open HMICP file: " + path);
    }
    
    // Read header
    HMICPHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(HMICPHeader));
    
    // Verify magic
    if (strncmp(header.magic, "HMICP", 5) != 0) {
        throw runtime_error("Invalid HMICP file - magic header mismatch!");
    }
    
    cout << "[DEBUG] ✅ Valid HMICP file detected!" << endl;
    cout << "[DEBUG] 📊 Version: " << (int)header.version << endl;
    cout << "[DEBUG] 📊 Dimensions: " << header.width << "x" << header.height << endl;
    cout << "[DEBUG] 📊 FPS: " << header.fps << endl;
    cout << "[DEBUG] 📊 Frames: " << header.totalFrames << endl;
    cout << "[DEBUG] 📊 Loop: " << (header.loop ? "YES" : "NO") << endl;
    
    // Calculate frame size
    size_t pixelsPerFrame = header.width * header.height;
    size_t bytesPerFrame = pixelsPerFrame * sizeof(RGBA);
    
    cout << "[DEBUG] 💾 Bytes per frame: " << bytesPerFrame << endl;
    cout << "[DEBUG] 💾 Total pixel data: " << (bytesPerFrame * header.totalFrames) << " bytes" << endl;
    
    // Read all frames
    vector<vector<RGBA>> frames(header.totalFrames);
    
    for (uint32_t f = 0; f < header.totalFrames; f++) {
        frames[f].resize(pixelsPerFrame);
        file.read(reinterpret_cast<char*>(frames[f].data()), bytesPerFrame);
        
        if (!file.good() && f < header.totalFrames - 1) {
            throw runtime_error("Failed to read frame " + to_string(f + 1));
        }
        
        if ((f + 1) % 10 == 0 || f == 0 || f == header.totalFrames - 1) {
            cout << "[DEBUG] 📥 Loaded frame " << (f + 1) << "/" << header.totalFrames << endl;
        }
    }
    
    file.close();
    
    cout << "[DEBUG] 🔥 ALL FRAMES LOADED - FILE READ COMPLETE!! 💪" << endl;
    
    // 🔍 DEBUG: Check first few pixels to see if we have actual data
    if (!frames.empty() && !frames[0].empty()) {
        cout << "[DEBUG] 🔍 First 10 pixels of frame 1:" << endl;
        for (int i = 0; i < min(10, (int)frames[0].size()); i++) {
            RGBA p = frames[0][i];
            cout << "[DEBUG]   Pixel " << i << ": RGBA(" 
                 << (int)p.r << "," << (int)p.g << "," << (int)p.b << "," << (int)p.a << ")" << endl;
        }
    }
    
    return {header, frames};
}

// 🌀 LOAD HMICP7 FILE (COMPRESSED)
pair<HMICPHeader, vector<vector<RGBA>>> loadHMICP7(const string& path) {
    cout << "[DEBUG] 🌀 Loading compressed HMICP7 file: " << path << endl;
    
    // Read compressed data
    ifstream file(path, ios::binary | ios::ate);
    if (!file.is_open()) {
        throw runtime_error("Failed to open HMICP7 file: " + path);
    }
    
    streamsize compressedSize = file.tellg();
    file.seekg(0, ios::beg);
    vector<char> compressed(compressedSize);
    
    if (!file.read(compressed.data(), compressedSize)) {
        throw runtime_error("Failed to read HMICP7 file");
    }
    file.close();
    
    cout << "[DEBUG] 💾 Read " << compressedSize << " bytes of compressed data" << endl;
    
    // Decompress
    unsigned long long decompSize = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    
    if (decompSize == ZSTD_CONTENTSIZE_ERROR) {
        throw runtime_error("Not a valid Zstd frame!");
    }
    if (decompSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw runtime_error("Decompressed size unknown!");
    }
    
    vector<char> decompressed(decompSize);
    
    size_t result = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        compressed.data(), compressed.size()
    );
    
    if (ZSTD_isError(result)) {
        throw runtime_error(string("Zstd decompression failed: ") + ZSTD_getErrorName(result));
    }
    
    cout << "[DEBUG] 🔥 Decompressed " << compressedSize << " → " << result 
         << " bytes (Zstd ATE THAT!!) 💯" << endl;
    
    // Write to temp file
    string tempFile = ".temp_hmicp_decompressed";
    ofstream temp(tempFile, ios::binary);
    temp.write(decompressed.data(), decompressed.size());
    temp.close();
    
    // Load using normal HMICP loader
    auto data = loadHMICP(tempFile);
    
    // Cleanup
    remove(tempFile.c_str());
    
    return data;
}

// 🎨 RENDER FRAME TO SDL TEXTURE
void renderFrameToTexture(SDL_Renderer* ren, SDL_Texture* tex, const vector<RGBA>& frame, int width, int height) {
    void* pixels;
    int pitch;
    
    if (SDL_LockTexture(tex, nullptr, &pixels, &pitch) < 0) {
        throw runtime_error(string("Failed to lock texture: ") + SDL_GetError());
    }
    
    // Copy pixel data directly
    // SDL expects RGBA in the same format we have, so this is INSTANT 🚀
    uint32_t* dest = static_cast<uint32_t*>(pixels);
    const uint32_t* src = reinterpret_cast<const uint32_t*>(frame.data());
    
    for (int y = 0; y < height; y++) {
        memcpy(dest + y * (pitch / 4), src + y * width, width * sizeof(uint32_t));
    }
    
    SDL_UnlockTexture(tex);
}

int main() {
    cout << "🔥🔥🔥 HMICP VIEWER - ZERO PARSING PURE SPEED EDITION 🔥🔥🔥" << endl;
    cout << "This viewer goes DIFFERENT - no parsing, just PURE PIXEL YEETING!! 💨💨" << endl;
    cout << endl;
    
    string path;
    cout << "Enter HMICP/HMICP7 file path: ";
    getline(cin, path);
    
    try {
        // Detect file type
        bool isCompressed = false;
        if (path.size() >= 7) {
            string ext = path.substr(path.size() - 7);
            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            isCompressed = (ext == ".hmicp7");
        }
        
        // Load file
        HMICPHeader header;
        vector<vector<RGBA>> frames;
        
        if (isCompressed) {
            tie(header, frames) = loadHMICP7(path);
        } else {
            tie(header, frames) = loadHMICP(path);
        }
        
        // Init SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw runtime_error(string("SDL init failed: ") + SDL_GetError());
        }
        
        // Calculate window size (with screen bounds)
        SDL_DisplayMode dm;
        SDL_GetCurrentDisplayMode(0, &dm);
        
        int MAX_WINDOW_WIDTH = (int)(dm.w * 0.9);
        int MAX_WINDOW_HEIGHT = (int)(dm.h * 0.9);
        
        float scale_by_width = (float)MAX_WINDOW_WIDTH / (float)header.width;
        float scale_by_height = (float)MAX_WINDOW_HEIGHT / (float)header.height;
        float scale = min(scale_by_width, scale_by_height);
        
        int pixelSize = (scale >= 1.0f) ? (int)floor(scale) : 1;
        if (pixelSize < 1) pixelSize = 1;
        
        int windowWidth = header.width * pixelSize;
        int windowHeight = header.height * pixelSize;
        
        // Enforce minimum window size so it's actually visible!!
        int MIN_WINDOW_HEIGHT = 100;
        int MIN_WINDOW_WIDTH = 100;
        
        if (windowHeight < MIN_WINDOW_HEIGHT) {
            float heightScale = (float)MIN_WINDOW_HEIGHT / (float)windowHeight;
            windowHeight = MIN_WINDOW_HEIGHT;
            windowWidth = (int)(windowWidth * heightScale);
            cout << "[DEBUG] ⚠️ Window too small! Scaling up to minimum size" << endl;
        }
        
        if (windowWidth < MIN_WINDOW_WIDTH) {
            windowWidth = MIN_WINDOW_WIDTH;
        }
        
        if (windowWidth > MAX_WINDOW_WIDTH) windowWidth = MAX_WINDOW_WIDTH;
        if (windowHeight > MAX_WINDOW_HEIGHT) windowHeight = MAX_WINDOW_HEIGHT;
        
        cout << "[DEBUG] 🖥️ Window: " << windowWidth << "x" << windowHeight 
             << " (scale: " << scale << ", pixel size: " << pixelSize << ")" << endl;
        
        // Create window and renderer
        SDL_Window* win = SDL_CreateWindow(
            "HMICP Viewer (BLOB EDITION - ZERO PARSING!!)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            windowWidth, windowHeight,
            SDL_WINDOW_SHOWN
        );
        
        if (!win) {
            throw runtime_error(string("Failed to create window: ") + SDL_GetError());
        }
        
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) {
            throw runtime_error(string("Failed to create renderer: ") + SDL_GetError());
        }
        
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        
        // Create texture for frame rendering
        // Use ABGR8888 to match our RGBA byte order!!
        SDL_Texture* frameTex = SDL_CreateTexture(
            ren,
            SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_STREAMING,
            header.width,
            header.height
        );
        
        if (!frameTex) {
            throw runtime_error(string("Failed to create texture: ") + SDL_GetError());
        }
        
        SDL_SetTextureBlendMode(frameTex, SDL_BLENDMODE_BLEND);
        
        cout << "[DEBUG] 🎬 STARTING PLAYBACK!! This is gonna be SMOOTH fr fr 💯" << endl;
        
        bool running = true;
        uint32_t currentFrame = 0;
        auto lastFrameTime = chrono::steady_clock::now();
        int frameDelay = (header.fps > 0) ? (1000 / header.fps) : 500;
        
        while (running) {
            // Handle events
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    running = false;
                }
                else if (e.type == SDL_KEYDOWN) {
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_q:
                            running = false;
                            break;
                        case SDLK_SPACE:
                            // Pause/resume (not implemented, but could be added)
                            break;
                        case SDLK_r:
                            currentFrame = 0;
                            cout << "[DEBUG] 🔄 Reset to frame 1" << endl;
                            break;
                    }
                }
            }
            
            // Timing
            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastFrameTime).count();
            
            if (elapsed >= frameDelay) {
                // Clear screen with a DARK GRAY background so light images are visible
                SDL_SetRenderDrawColor(ren, 32, 32, 32, 255);
                SDL_RenderClear(ren);
                
                // 🔍 DEBUG: Check what we're about to render
                if (currentFrame == 0) {
                    int nonZeroPixels = 0;
                    for (const auto& p : frames[currentFrame]) {
                        if (p.r != 0 || p.g != 0 || p.b != 0 || p.a != 0) {
                            nonZeroPixels++;
                        }
                    }
                    cout << "[DEBUG] 🎨 Frame has " << nonZeroPixels << " non-zero pixels out of " 
                         << frames[currentFrame].size() << " total" << endl;
                }
                
                // Render current frame to texture (THIS IS WHERE THE MAGIC HAPPENS 🔥)
                renderFrameToTexture(ren, frameTex, frames[currentFrame], header.width, header.height);
                
                // Draw texture to screen (scaled)
                SDL_RenderCopy(ren, frameTex, nullptr, nullptr);
                
                SDL_RenderPresent(ren);
                
                // Debug output (only occasionally to avoid spam)
                if (currentFrame == 0 || (currentFrame + 1) % 30 == 0 || currentFrame == header.totalFrames - 1) {
                    cout << "[DEBUG] 🎬 Rendered frame " << (currentFrame + 1) << "/" << header.totalFrames 
                         << " (elapsed: " << elapsed << "ms)" << endl;
                }
                
                // Next frame
                currentFrame++;
                if (currentFrame >= header.totalFrames) {
                    if (header.loop) {
                        currentFrame = 0;
                        cout << "[DEBUG] 🔄 Looping back to frame 1!" << endl;
                    } else {
                        cout << "[DEBUG] 🏁 Animation complete (no loop)" << endl;
                        running = false;
                    }
                }
                
                lastFrameTime = now;
            }
            
            // Small sleep to prevent CPU melting
            this_thread::sleep_for(chrono::milliseconds(1));
        }
        
        // Cleanup
        SDL_DestroyTexture(frameTex);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        
        cout << endl;
        cout << "💥💥💥 VIEWER CLOSED - THAT WAS SMOOTH AS BUTTER!! 💥💥💥" << endl;
        cout << "No parsing overhead, just PURE PIXEL YEET ENERGY!! 🚀🚀" << endl;
        
    } catch (const exception& e) {
        cerr << "❌ ERROR: " << e.what() << endl;
        SDL_Quit();
        return 1;
    }
    
    return 0;
}