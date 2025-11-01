#include <SDL2/SDL.h>
#include "hmicx.h"
#include <zstd.h>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <cstring>

using namespace std;
using namespace HMICX;

// global chaos config
int WIDTH = 5, HEIGHT = 5, FPS = 2, TOTAL_FRAMES = 9;
bool LOOP = true;
int PIXEL_SIZE = 100;

// 🔥 ZSTD DECOMPRESSION FUNCTION 🔥
string decompress_zstd(const vector<char>& compressed_data) {
    // Get decompressed size
    unsigned long long const decompressed_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
    
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        throw runtime_error("❌ Not a valid Zstd frame!");
    }
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw runtime_error("❌ Decompressed size unknown (streaming not supported)");
    }
    
    // Allocate buffer for decompressed data
    vector<char> decompressed(decompressed_size);
    
    // Decompress
    size_t const result = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        compressed_data.data(), compressed_data.size()
    );
    
    if (ZSTD_isError(result)) {
        throw runtime_error(string("❌ Zstd decompression failed: ") + ZSTD_getErrorName(result));
    }
    
    cout << "[DEBUG] 🌀 Decompressed " << compressed_data.size() << " bytes → " 
         << result << " bytes (ZSTD ATE THAT FR FR) 🔥" << endl;
    
    return string(decompressed.begin(), decompressed.end());
}

// 🎨 Color parsing (convert HMICX color string to SDL_Color)
SDL_Color parse_color(const string& s) {
    SDL_Color c = {255, 255, 255, 255};
    string str = s;
    transform(str.begin(), str.end(), str.begin(), ::tolower);

    if (str.rfind("#", 0) == 0 && str.size() == 7) {
        int r = stoi(str.substr(1, 2), nullptr, 16);
        int g = stoi(str.substr(3, 2), nullptr, 16);
        int b = stoi(str.substr(5, 2), nullptr, 16);
        c = {Uint8(r), Uint8(g), Uint8(b), 255};
    } else if (str.find("rgb(") == 0) {
        int r, g, b;
        if (sscanf(str.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3)
            c = {Uint8(r), Uint8(g), Uint8(b), 255};
    }
    return c;
}

// 🔥 Convert HMICX header to globals
void apply_header(const map<string, string>& header) {
    for (auto& [key, val] : header) {
        string k = key;
        transform(k.begin(), k.end(), k.begin(), ::toupper);
        
        if (k == "DISPLAY") {
            string v = val;
            transform(v.begin(), v.end(), v.begin(), ::tolower);
            int w = 0, h = 0;
            if (sscanf(v.c_str(), "%dx%d", &w, &h) == 2) {
                WIDTH = w;
                HEIGHT = h;
                cout << "[DEBUG] ✅ Parsed dimensions: " << WIDTH << "x" << HEIGHT << endl;
            }
        }
        else if (k == "FPS") FPS = stoi(val);
        else if (k == "F") TOTAL_FRAMES = stoi(val);
        else if (k == "LOOP") LOOP = (val == "Y" || val == "1");
        
        cout << "[DEBUG] " << k << "=" << val << endl;
    }
}

// 🎬 Convert HMICX commands to SDL rendering data
struct RenderCommand {
    int start, end;
    vector<pair<int, int>> pixels;
    SDL_Color color;
};

vector<RenderCommand> convert_commands(const vector<Command>& hmicx_cmds) {
    vector<RenderCommand> render_cmds;
    
    for (auto& cmd : hmicx_cmds) {
        RenderCommand rc;
        rc.start = cmd.start;
        rc.end = cmd.end;
        rc.color = parse_color(cmd.color);
        
        // Convert HMICX::Pixel to pair<int,int>
        for (auto& px : cmd.pixels) {
            rc.pixels.push_back({px.x, px.y});
        }
        
        render_cmds.push_back(rc);
    }
    
    return render_cmds;
}

// 💀 Check if file is HMIC7 based on extension
bool is_hmic7(const string& path) {
    if (path.size() < 6) return false;
    string ext = path.substr(path.size() - 6);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".hmic7";
}

int main() {
    cout << "🌀 HMIC SDL2 VIEWER (POWERED BY HMICX + ZSTD) 🌀" << endl;
    cout << "Enter file path: ";
    string path;
    getline(cin, path);

    // 🔥🔥 DETECT AND HANDLE HMIC7 FILES!! 🔥🔥
    try {
        string parse_path = path;
        bool is_compressed = is_hmic7(path);
        string temp_file = ".hmic_temp_decompressed.hmic";
        
        if (is_compressed) {
            cout << "[DEBUG] 🌀 Detected HMIC7 file — preparing to DECOMPRESS 🔥" << endl;
            
            // Read compressed file
            ifstream file(path, ios::binary | ios::ate);
            if (!file.is_open()) {
                throw runtime_error("❌ Failed to open HMIC7 file: " + path);
            }
            
            streamsize size = file.tellg();
            file.seekg(0, ios::beg);
            
            vector<char> compressed_buffer(size);
            if (!file.read(compressed_buffer.data(), size)) {
                throw runtime_error("❌ Failed to read HMIC7 file");
            }
            
            cout << "[DEBUG] 💾 Read " << size << " bytes of compressed data" << endl;
            
            // Decompress using Zstd
            string hmic_data = decompress_zstd(compressed_buffer);
            
            cout << "[DEBUG] ✅ HMIC7 decompressed successfully — Zstd went CRAZY no cap 🚀" << endl;
            
            // Write to temp file for parser
            ofstream temp(temp_file);
            if (!temp.is_open()) {
                throw runtime_error("❌ Failed to create temp file");
            }
            temp << hmic_data;
            temp.close();
            
            parse_path = temp_file;
            cout << "[DEBUG] 📝 Wrote decompressed data to temp file" << endl;
        }
        
        // 🔥 Parse using HMICX library (always from file path)
        Parser parser(parse_path);
        parser.parse();
        
        auto header = parser.getHeader();
        apply_header(header);
        
        cout << "[DEBUG] 🔥 FINAL WIDTH=" << WIDTH << " HEIGHT=" << HEIGHT << " 🔥" << endl;
        
        auto hmicx_cmds = parser.getCommands();
        auto cmds = convert_commands(hmicx_cmds);
        
        cout << "[DEBUG] total commands parsed: " << cmds.size() << endl;

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            cerr << "SDL init fail: " << SDL_GetError() << endl;
            return 1;
        }

        // 🔥🔥 SMART WINDOW SIZE CALCULATION THAT ACTUALLY WORKS FOR 4K!! 🔥🔥
        SDL_DisplayMode dm;
        SDL_GetCurrentDisplayMode(0, &dm);
        int SCREEN_WIDTH = dm.w;
        int SCREEN_HEIGHT = dm.h;
        
        // Leave some margin (90% of screen)
        int MAX_WINDOW_WIDTH = (int)(SCREEN_WIDTH * 0.9);
        int MAX_WINDOW_HEIGHT = (int)(SCREEN_HEIGHT * 0.9);
        
        cout << "[DEBUG] 🖥️ Screen: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT 
             << " → Max window: " << MAX_WINDOW_WIDTH << "x" << MAX_WINDOW_HEIGHT << endl;
        
        // ⚡ FIXED: Calculate scale as float first!
        float scale_by_width = (float)MAX_WINDOW_WIDTH / (float)WIDTH;
        float scale_by_height = (float)MAX_WINDOW_HEIGHT / (float)HEIGHT;
        float scale = (scale_by_width < scale_by_height) ? scale_by_width : scale_by_height;
        
        // For images smaller than screen, use integer pixel sizes (1, 2, 3...)
        // For images bigger than screen, use fractional scale (0.5, 0.25...)
        if (scale >= 1.0f) {
            PIXEL_SIZE = (int)floor(scale);
            if (PIXEL_SIZE < 1) PIXEL_SIZE = 1;
        } else {
            // Image is too big - we need to scale DOWN
            PIXEL_SIZE = 1; // we'll use SDL's scaling instead
        }
        
        int RENDER_WIDTH = WIDTH * PIXEL_SIZE;
        int RENDER_HEIGHT = HEIGHT * PIXEL_SIZE;
        
        // ⚡ If image is still too big, clamp to max window size
        if (RENDER_WIDTH > MAX_WINDOW_WIDTH || RENDER_HEIGHT > MAX_WINDOW_HEIGHT) {
            RENDER_WIDTH = MAX_WINDOW_WIDTH;
            RENDER_HEIGHT = MAX_WINDOW_HEIGHT;
            cout << "[DEBUG] ⚠️ Image too large! Clamping to screen size" << endl;
        }
        
        cout << "[DEBUG] 💎 Window: " << RENDER_WIDTH << "x" << RENDER_HEIGHT 
             << " (scale: " << scale << ", pixel size: " << PIXEL_SIZE << ") 💎" << endl;
        
        SDL_Window* win = SDL_CreateWindow("HMIC Viewer (HMICX + ZSTD Edition)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            RENDER_WIDTH, RENDER_HEIGHT, 0);
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

        // ⚡ For large images, use render target to scale properly
        SDL_Texture* renderTarget = nullptr;
        bool useScaling = (WIDTH > MAX_WINDOW_WIDTH || HEIGHT > MAX_WINDOW_HEIGHT);
        
        if (useScaling) {
            renderTarget = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, 
                                            SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
            cout << "[DEBUG] 🎨 Using render target for proper scaling" << endl;
        }

        bool running = true;
        int frame = 1;

        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e))
                if (e.type == SDL_QUIT) running = false;

            if (useScaling) {
                // Render to texture at native resolution
                SDL_SetRenderTarget(ren, renderTarget);
            }

            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);

            for (auto& c : cmds) {
                if (frame >= c.start && frame <= c.end) {
                    SDL_SetRenderDrawColor(ren, c.color.r, c.color.g, c.color.b, 255);
                    for (auto& p : c.pixels) {
                        if (useScaling) {
                            // Draw at 1:1 pixel scale
                            SDL_Rect r = {p.first - 1, p.second - 1, 1, 1};
                            SDL_RenderFillRect(ren, &r);
                        } else {
                            // Draw with pixel size scaling
                            SDL_Rect r = {(p.first - 1) * PIXEL_SIZE,
                                          (p.second - 1) * PIXEL_SIZE,
                                          PIXEL_SIZE, PIXEL_SIZE};
                            SDL_RenderFillRect(ren, &r);
                        }
                    }
                }
            }

            if (useScaling) {
                // Copy scaled texture to screen
                SDL_SetRenderTarget(ren, nullptr);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                SDL_RenderClear(ren);
                SDL_RenderCopy(ren, renderTarget, nullptr, nullptr);
            }

            SDL_RenderPresent(ren);
            this_thread::sleep_for(chrono::milliseconds(1000 / max(1, FPS)));

            frame++;
            if (frame > TOTAL_FRAMES) {
                if (LOOP) frame = 1;
                else running = false;
            }
        }

        if (renderTarget) SDL_DestroyTexture(renderTarget);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        
        // 🧹 Clean up temp file if we made one
        if (is_compressed) {
            remove(temp_file.c_str());
            cout << "[DEBUG] 🗑️ Cleaned up temp file" << endl;
        }
        
        cout << "💥 CHAOS ENDED SAFELY 💥" << endl;
        
    } catch (const exception& e) {
        cerr << "[ERROR] " << e.what() << endl;
        return 1;
    }

    return 0;
}