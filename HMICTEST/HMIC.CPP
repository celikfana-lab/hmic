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
int WIDTH = 5, HEIGHT = 5, FPS = 2, TOTAL_FRAMES = 1;
bool LOOP = true;
int PIXEL_SIZE = 100;

// 🔥 ZSTD DECOMPRESSION FUNCTION 🔥
string decompress_zstd(const vector<char>& compressed_data) {
    unsigned long long const decompressed_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
    
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        throw runtime_error("❌ Not a valid Zstd frame!");
    }
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw runtime_error("❌ Decompressed size unknown (streaming not supported)");
    }
    
    vector<char> decompressed(decompressed_size);
    
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

// 🎨 UPGRADED COLOR PARSING WITH RGBA SUPPORT!! 🎨
SDL_Color parse_color(const string& s) {
    SDL_Color c = {255, 255, 255, 255};
    string str = s;
    transform(str.begin(), str.end(), str.begin(), ::tolower);

    if (str.rfind("#", 0) == 0 && str.size() == 7) {
        int r = stoi(str.substr(1, 2), nullptr, 16);
        int g = stoi(str.substr(3, 2), nullptr, 16);
        int b = stoi(str.substr(5, 2), nullptr, 16);
        c = {Uint8(r), Uint8(g), Uint8(b), 255};
        cout << "[DEBUG] 🎨 Parsed hex color: " << s << " → rgb(" 
             << (int)c.r << "," << (int)c.g << "," << (int)c.b << ")" << endl;
    } 
    else if (str.find("rgba(") == 0) {
        int r, g, b, a;
        if (sscanf(str.c_str(), "rgba(%d,%d,%d,%d)", &r, &g, &b, &a) == 4) {
            c = {Uint8(r), Uint8(g), Uint8(b), Uint8(a)};
            cout << "[DEBUG] 🎨✨ Parsed RGBA color: " << s << " → rgba(" 
                 << (int)c.r << "," << (int)c.g << "," << (int)c.b << "," << (int)c.a 
                 << ") - TRANSPARENCY ENABLED!! 💎" << endl;
        }
    }
    else if (str.find("rgb(") == 0) {
        int r, g, b;
        if (sscanf(str.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3) {
            c = {Uint8(r), Uint8(g), Uint8(b), 255};
            cout << "[DEBUG] 🎨 Parsed RGB color: " << s << " → rgb(" 
                 << (int)c.r << "," << (int)c.g << "," << (int)c.b << ")" << endl;
        }
    }
    
    return c;
}

void apply_header(const map<string, string>& header) {
    cout << "[DEBUG] 🔍 PARSING HEADER - FULL DUMP:" << endl;
    for (auto& [key, val] : header) {
        cout << "[DEBUG]   RAW: '" << key << "' = '" << val << "'" << endl;
    }
    
    for (auto& [key, val] : header) {
        string k = key;
        transform(k.begin(), k.end(), k.begin(), ::toupper);
        
        if (k == "DISPLAY") {
            string v = val;
            transform(v.begin(), v.end(), v.begin(), ::tolower);
            int w = 0, h = 0;
            if (sscanf(v.c_str(), "%dx%d", &w, &h) == 2 || sscanf(v.c_str(), "%dX%d", &w, &h) == 2) {
                WIDTH = w;
                HEIGHT = h;
                cout << "[DEBUG] ✅ Parsed dimensions: " << WIDTH << "x" << HEIGHT << endl;
            } else {
                cout << "[DEBUG] ⚠️ Failed to parse DISPLAY value: '" << v << "'" << endl;
            }
        }
        else if (k == "FPS") {
            FPS = stoi(val);
            cout << "[DEBUG] ✅ FPS=" << FPS << endl;
        }
        else if (k == "F") {
            TOTAL_FRAMES = stoi(val);
            cout << "[DEBUG] ✅ TOTAL_FRAMES=" << TOTAL_FRAMES << endl;
        }
        else if (k == "LOOP") {
            LOOP = (val == "Y" || val == "y" || val == "1");
            cout << "[DEBUG] ✅ LOOP=" << (LOOP ? "YES" : "NO") << endl;
        }
        else {
            cout << "[DEBUG] ⚠️ Unknown header key: " << k << endl;
        }
    }
    
    cout << "[DEBUG] 🎯 FINAL CONFIG: " << WIDTH << "x" << HEIGHT 
         << " @ " << FPS << " FPS, " << TOTAL_FRAMES << " frames, "
         << "LOOP=" << (LOOP ? "YES" : "NO") << endl;
}

struct RenderCommand {
    int start, end;
    vector<pair<int, int>> pixels;
    SDL_Color color;
};

vector<RenderCommand> convert_commands(const vector<Command>& hmicx_cmds) {
    vector<RenderCommand> render_cmds;
    
    cout << "[DEBUG] 🎨 Converting " << hmicx_cmds.size() << " HMICX commands..." << endl;
    
    int multi_frame_count = 0;
    for (auto& cmd : hmicx_cmds) {
        RenderCommand rc;
        rc.start = cmd.start;
        rc.end = cmd.end;
        rc.color = parse_color(cmd.color);
        
        if (cmd.start != cmd.end) {
            multi_frame_count++;
        }
        
        for (auto& px : cmd.pixels) {
            rc.pixels.push_back({px.x, px.y});
        }
        
        render_cmds.push_back(rc);
    }
    
    cout << "[DEBUG] 💎 Found " << multi_frame_count << " multi-frame commands!" << endl;
    
    return render_cmds;
}

bool is_hmic7(const string& path) {
    if (path.size() < 6) return false;
    string ext = path.substr(path.size() - 6);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".hmic7";
}

int main() {
    cout << "🌀 HMIC SDL2 VIEWER (POWERED BY HMICX + ZSTD + RGBA!!) 🌀" << endl;
    cout << "Enter file path: ";
    string path;
    getline(cin, path);

    try {
        string parse_path = path;
        bool is_compressed = is_hmic7(path);
        string temp_file = ".hmic_temp_decompressed.hmic";
        
        if (is_compressed) {
            cout << "[DEBUG] 🌀 Detected HMIC7 file — preparing to DECOMPRESS 🔥" << endl;
            
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
            
            string hmic_data = decompress_zstd(compressed_buffer);
            
            cout << "[DEBUG] ✅ HMIC7 decompressed successfully — Zstd went CRAZY no cap 🚀" << endl;
            
            ofstream temp(temp_file);
            if (!temp.is_open()) {
                throw runtime_error("❌ Failed to create temp file");
            }
            temp << hmic_data;
            temp.close();
            
            parse_path = temp_file;
            cout << "[DEBUG] 📝 Wrote decompressed data to temp file" << endl;
        }
        
        Parser parser(parse_path);
        parser.parse();
        
        auto header = parser.getHeader();
        apply_header(header);
        
        auto hmicx_cmds = parser.getCommands();
        auto cmds = convert_commands(hmicx_cmds);
        
        cout << "[DEBUG] 📊 Total commands: " << cmds.size() << endl;

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            cerr << "SDL init fail: " << SDL_GetError() << endl;
            return 1;
        }

        SDL_DisplayMode dm;
        SDL_GetCurrentDisplayMode(0, &dm);
        int SCREEN_WIDTH = dm.w;
        int SCREEN_HEIGHT = dm.h;
        
        int MAX_WINDOW_WIDTH = (int)(SCREEN_WIDTH * 0.9);
        int MAX_WINDOW_HEIGHT = (int)(SCREEN_HEIGHT * 0.9);
        
        cout << "[DEBUG] 🖥️ Screen: " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT 
             << " → Max window: " << MAX_WINDOW_WIDTH << "x" << MAX_WINDOW_HEIGHT << endl;
        
        float scale_by_width = (float)MAX_WINDOW_WIDTH / (float)WIDTH;
        float scale_by_height = (float)MAX_WINDOW_HEIGHT / (float)HEIGHT;
        float scale = (scale_by_width < scale_by_height) ? scale_by_width : scale_by_height;
        
        if (scale >= 1.0f) {
            PIXEL_SIZE = (int)floor(scale);
            if (PIXEL_SIZE < 1) PIXEL_SIZE = 1;
        } else {
            PIXEL_SIZE = 1;
        }
        
        int RENDER_WIDTH = WIDTH * PIXEL_SIZE;
        int RENDER_HEIGHT = HEIGHT * PIXEL_SIZE;
        
        if (RENDER_WIDTH > MAX_WINDOW_WIDTH || RENDER_HEIGHT > MAX_WINDOW_HEIGHT) {
            RENDER_WIDTH = MAX_WINDOW_WIDTH;
            RENDER_HEIGHT = MAX_WINDOW_HEIGHT;
            cout << "[DEBUG] ⚠️ Image too large! Clamping to screen size" << endl;
        }
        
        cout << "[DEBUG] 💎 Window: " << RENDER_WIDTH << "x" << RENDER_HEIGHT 
             << " (scale: " << scale << ", pixel size: " << PIXEL_SIZE << ") 💎" << endl;
        
        SDL_Window* win = SDL_CreateWindow("HMIC Viewer (HMICX + ZSTD + RGBA Edition)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            RENDER_WIDTH, RENDER_HEIGHT, 0);
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        cout << "[DEBUG] 🌈 Alpha blending ENABLED - transparency mode ACTIVATED!! ✨" << endl;

        SDL_Texture* renderTarget = nullptr;
        bool useScaling = (WIDTH > MAX_WINDOW_WIDTH || HEIGHT > MAX_WINDOW_HEIGHT);
        
        if (useScaling) {
            renderTarget = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, 
                                            SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
            SDL_SetTextureBlendMode(renderTarget, SDL_BLENDMODE_BLEND);
            cout << "[DEBUG] 🎨 Using render target for proper scaling (with alpha support!)" << endl;
        }

        bool running = true;
        int frame = 1;
        
        cout << "[DEBUG] 🎬 STARTING ANIMATION LOOP! Frame 1/" << TOTAL_FRAMES << endl;

        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e))
                if (e.type == SDL_QUIT) running = false;

            if (useScaling) {
                SDL_SetRenderTarget(ren, renderTarget);
            }

            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);

            int pixels_drawn = 0;
            for (auto& c : cmds) {
                if (frame >= c.start && frame <= c.end) {
                    SDL_SetRenderDrawColor(ren, c.color.r, c.color.g, c.color.b, c.color.a);
                    for (auto& p : c.pixels) {
                        if (useScaling) {
                            SDL_Rect r = {p.first - 1, p.second - 1, 1, 1};
                            SDL_RenderFillRect(ren, &r);
                        } else {
                            SDL_Rect r = {(p.first - 1) * PIXEL_SIZE,
                                          (p.second - 1) * PIXEL_SIZE,
                                          PIXEL_SIZE, PIXEL_SIZE};
                            SDL_RenderFillRect(ren, &r);
                        }
                        pixels_drawn++;
                    }
                }
            }

            if (useScaling) {
                SDL_SetRenderTarget(ren, nullptr);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                SDL_RenderClear(ren);
                SDL_RenderCopy(ren, renderTarget, nullptr, nullptr);
            }

            SDL_RenderPresent(ren);
            
            if (frame == 1 || frame % 10 == 0 || frame == TOTAL_FRAMES) {
                cout << "[DEBUG] 🎬 Frame " << frame << "/" << TOTAL_FRAMES 
                     << " - Drew " << pixels_drawn << " pixels" << endl;
            }
            
            this_thread::sleep_for(chrono::milliseconds(1000 / max(1, FPS)));

            frame++;
            if (frame > TOTAL_FRAMES) {
                if (LOOP) {
                    cout << "[DEBUG] 🔄 Looping back to frame 1!" << endl;
                    frame = 1;
                } else {
                    cout << "[DEBUG] 🏁 Animation complete (no loop)" << endl;
                    running = false;
                }
            }
        }

        if (renderTarget) SDL_DestroyTexture(renderTarget);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        
        if (is_compressed) {
            remove(temp_file.c_str());
            cout << "[DEBUG] 🗑️ Cleaned up temp file" << endl;
        }
        
        cout << "💥 CHAOS ENDED SAFELY (with beautiful transparency support!) 💥" << endl;
        
    } catch (const exception& e) {
        cerr << "[ERROR] " << e.what() << endl;
        return 1;
    }

    return 0;
}