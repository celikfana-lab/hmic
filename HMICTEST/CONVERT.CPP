#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>

// 🌐 ENABLE WEBP SUPPORT!!
#define STBI_SUPPORT_WEBP
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// 🚀 LIBWEBP FOR SUPERIOR WEBP DECODING!!
#include <webp/decode.h>

#include <zstd.h>

namespace fs = std::filesystem;

std::mutex cout_mutex;
std::atomic<int> processed_rows{0};

// 🎨 RGBA STRUCT WITH ALPHA CHANNEL SUPPORT!!
struct RGBA {
    uint8_t r, g, b, a;
    
    bool operator<(const RGBA& other) const {
        if (r != other.r) return r < other.r;
        if (g != other.g) return g < other.g;
        if (b != other.b) return b < other.b;
        return a < other.a;
    }
    
    bool operator==(const RGBA& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};

struct Command {
    std::string cmd;
    int x, end_x, y;
    
    bool operator==(const Command& other) const {
        return cmd == other.cmd && x == other.x && end_x == other.end_x && y == other.y;
    }

    bool operator<(const Command& other) const {
        if (y != other.y) return y < other.y;
        if (x != other.x) return x < other.x;
        if (end_x != other.end_x) return end_x < other.end_x;
        return cmd < other.cmd;
    }
};

namespace std {
    template<>
    struct hash<Command> {
        size_t operator()(const Command& c) const {
            return hash<string>()(c.cmd) ^ (hash<int>()(c.x) << 1) ^ 
                   (hash<int>()(c.end_x) << 2) ^ (hash<int>()(c.y) << 3);
        }
    };
}

// 🎯 Convert frame lists to frame ranges
std::string frames_to_range_string(const std::vector<int>& frames) {
    if (frames.empty()) return "";
    if (frames.size() == 1) return std::to_string(frames[0]);
    
    std::vector<std::string> ranges;
    int start = frames[0];
    int end = frames[0];
    
    for (size_t i = 1; i < frames.size(); i++) {
        if (frames[i] == end + 1) {
            end = frames[i];
        } else {
            if (start == end) {
                ranges.push_back(std::to_string(start));
            } else {
                ranges.push_back(std::to_string(start) + "-" + std::to_string(end));
            }
            start = frames[i];
            end = frames[i];
        }
    }
    
    if (start == end) {
        ranges.push_back(std::to_string(start));
    } else {
        ranges.push_back(std::to_string(start) + "-" + std::to_string(end));
    }
    
    std::string result;
    for (size_t i = 0; i < ranges.size(); i++) {
        result += ranges[i];
        if (i < ranges.size() - 1) result += ",";
    }
    return result;
}

// 🔥 DETECT IMAGE FORMAT FROM FILE EXTENSION
std::string get_file_extension(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) return "";
    std::string ext = path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// 🌐 DEDICATED WEBP LOADER USING LIBWEBP (THE REAL DEAL!!)
bool load_webp_image(const std::string& path, int& w, int& h, std::vector<RGBA>& pixels) {
    std::cout << "🚀 Using libwebp for SUPERIOR WEBP decoding!! 💎\n";
    
    // Read entire file into memory
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open WEBP file\n";
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read((char*)buffer.data(), size)) {
        std::cerr << "❌ Failed to read WEBP file\n";
        return false;
    }
    file.close();
    
    // Decode WEBP with libwebp (RGBA mode activated!!)
    uint8_t* decoded = WebPDecodeRGBA(buffer.data(), buffer.size(), &w, &h);
    
    if (!decoded) {
        std::cerr << "❌ libwebp failed to decode - file might be corrupted\n";
        return false;
    }
    
    std::cout << "✅ WEBP decoded successfully: " << w << "x" << h << " with RGBA 🔥\n";
    
    // Convert to RGBA pixels
    pixels.resize(w * h);
    for (int i = 0; i < w * h; i++) {
        pixels[i] = {
            decoded[i * 4],
            decoded[i * 4 + 1],
            decoded[i * 4 + 2],
            decoded[i * 4 + 3]
        };
    }
    
    WebPFree(decoded);
    return true;
}

// 🚀 UNIVERSAL IMAGE LOADER - SUPPORTS ALL FORMATS!!
bool load_universal_image(const std::string& path, int& w, int& h, std::vector<RGBA>& pixels) {
    std::string ext = get_file_extension(path);
    
    std::cout << "🔍 Detected format: ." << ext << "\n";
    
    // 🌐 Special handling for WEBP using libwebp!!
    if (ext == "webp") {
        return load_webp_image(path, w, h, pixels);
    }
    
    // 🎨 All other formats use STB_IMAGE
    int channels;
    unsigned char* img_data = stbi_load(path.c_str(), &w, &h, &channels, 4); // Force RGBA
    
    if (!img_data) {
        std::cerr << "❌ STB_IMAGE failed: " << stbi_failure_reason() << "\n";
        std::cerr << "💡 Make sure file exists and is a valid image format\n";
        std::cerr << "📋 Supported: JPG, PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM, WEBP\n";
        return false;
    }
    
    std::cout << "✅ Loaded " << ext << " → " << w << "x" << h 
              << " (original: " << channels << " channels, converted to RGBA)\n";
    
    // Convert to RGBA pixels
    pixels.resize(w * h);
    for (int i = 0; i < w * h; i++) {
        pixels[i] = {
            img_data[i * 4],
            img_data[i * 4 + 1],
            img_data[i * 4 + 2],
            img_data[i * 4 + 3]
        };
    }
    
    stbi_image_free(img_data);
    return true;
}

// 🎬 UNIVERSAL GIF LOADER WITH PROPER ALPHA SUPPORT
bool load_gif_frames(const std::string& path, int& w, int& h, int& n_frames, int& fps,
                    std::vector<std::vector<RGBA>>& frames_data) {
    
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open GIF file\n";
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<unsigned char> buffer(size);
    if (!file.read((char*)buffer.data(), size)) {
        std::cerr << "❌ Failed to read GIF file\n";
        return false;
    }
    file.close();
    
    int channels, z = 0;
    int* delays = nullptr;
    
    // Load with 4 channels for RGBA!!
    unsigned char* img_data = stbi_load_gif_from_memory(buffer.data(), buffer.size(), 
                                                        &delays, &w, &h, &z, &channels, 4);
    
    if (!img_data) {
        std::cerr << "❌ Failed to load GIF: " << stbi_failure_reason() << "\n";
        std::cerr << "💡 STB might not fully support this GIF, try converting with imagemagick\n";
        return false;
    }
    
    n_frames = z;
    
    std::cout << "📊 GIF INFO: " << w << "x" << h << " @ " << n_frames << " frames (RGBA MODE 💎)\n";
    
    if (delays && delays[0] > 0) {
        fps = std::max(1, 1000 / delays[0]);
        std::cout << "🎬 Frame delay: " << delays[0] << "ms → " << fps << " FPS\n";
    } else {
        fps = 10;
        std::cout << "⚠️ No delay info, defaulting to 10 FPS\n";
    }
    
    // 🎨 EXTRACT ALL FRAMES WITH ALPHA!!
    for (int frame_idx = 0; frame_idx < n_frames; frame_idx++) {
        std::vector<RGBA> frame_pixels(w * h);
        
        int frame_offset = frame_idx * w * h * 4;
        
        for (int i = 0; i < w * h; i++) {
            frame_pixels[i] = {
                img_data[frame_offset + i * 4],
                img_data[frame_offset + i * 4 + 1],
                img_data[frame_offset + i * 4 + 2],
                img_data[frame_offset + i * 4 + 3]
            };
        }
        
        frames_data.push_back(frame_pixels);
        std::cout << "[DEBUG] 📦 Extracted frame " << (frame_idx + 1) << "/" << n_frames 
                  << " (WITH ALPHA!) ✅\n";
    }
    
    stbi_image_free(img_data);
    if (delays) free(delays);
    
    return true;
}

// 🚀 PROCESS ROWS IN PARALLEL WITH RGBA!!
void process_frame_rows_parallel(
    const std::vector<RGBA>& frame_pixels,
    int w, int h,
    int start_row, int end_row,
    std::map<RGBA, std::vector<Command>>* local_commands
) {
    for (int y = start_row; y < end_row; y++) {
        int x = 0;
        while (x < w) {
            RGBA pixel_color = frame_pixels[y * w + x];
            
            // Find horizontal run of same color
            int run_length = 1;
            while (x + run_length < w && frame_pixels[y * w + x + run_length] == pixel_color) {
                run_length++;
            }
            
            // Generate command
            int end_x = x + run_length - 1;
            std::string cmd;
            if (run_length == 1) {
                cmd = "P=" + std::to_string(x + 1) + "x" + std::to_string(y + 1);
            } else {
                cmd = "PL=" + std::to_string(x + 1) + "x" + std::to_string(y + 1) + 
                      "-" + std::to_string(end_x + 1) + "x" + std::to_string(y + 1);
            }
            
            (*local_commands)[pixel_color].push_back({cmd, x, end_x, y});
            x += run_length;
        }
        
        processed_rows++;
        if (processed_rows % 100 == 0) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[DEBUG] 🔥 Row " << processed_rows << "/" << h << "\n";
        }
    }
}

int main() {
    std::cout << "🔥🔥🔥 UNIVERSAL IMAGE CONVERTER - ALL FORMATS UNLOCKED 🔥🔥🔥\n";
    std::cout << "💎 SUPPORTS: JPG, PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM, WEBP 💎\n";
    std::cout << "🌐 WEBP: Using Google's libwebp library (THE GOAT!!) 🌐\n";
    std::cout << "💪 " << std::thread::hardware_concurrency() 
              << " CORES DETECTED + FULL RGBA SUPPORT = UNSTOPPABLE 💪\n\n";
    
    // 🩵 Load the sacred image/gif
    std::string img_path;
    std::cout << "Enter image file path: ";
    std::getline(std::cin, img_path);
    
    // Check if file exists
    if (!fs::exists(img_path)) {
        std::cerr << "❌ File not found: " << img_path << "\n";
        return 1;
    }
    
    // Detect format
    std::string ext = get_file_extension(img_path);
    bool is_gif = (ext == "gif");
    
    int w, h, n_frames = 1, fps = 1;
    bool loop = true;
    std::vector<std::vector<RGBA>> frames_data;
    
    if (is_gif) {
        std::cout << "\n🎬 GIF MODE ACTIVATED - MULTI-FRAME RGBA EDITION 🎬\n";
        
        if (!load_gif_frames(img_path, w, h, n_frames, fps, frames_data)) {
            return 1;
        }
        
        std::cout << "🎬 ANIMATED GIF: " << n_frames << " frames @ " << fps << " FPS 🔥\n\n";
        
        // 🔍 DIAGNOSTIC: Save first frame as PNG with alpha
        std::string debug_png = "debug_frame1_rgba.png";
        std::vector<uint8_t> png_data(w * h * 4);
        for (int i = 0; i < w * h; i++) {
            png_data[i * 4] = frames_data[0][i].r;
            png_data[i * 4 + 1] = frames_data[0][i].g;
            png_data[i * 4 + 2] = frames_data[0][i].b;
            png_data[i * 4 + 3] = frames_data[0][i].a;
        }
        stbi_write_png(debug_png.c_str(), w, h, 4, png_data.data(), w * 4);
        std::cout << "🔍 SAVED " << debug_png << " - CHECK IF ALPHA LOOKS CORRECT!! 💎\n\n";
        
    } else {
        // 📸 LOAD SINGLE IMAGE WITH UNIVERSAL FORMAT SUPPORT
        std::cout << "\n📸 Single frame image detected — UNIVERSAL FORMAT MODE 💚\n";
        std::cout << "🎨 Format: ." << ext << "\n\n";
        
        std::vector<RGBA> frame_pixels;
        if (!load_universal_image(img_path, w, h, frame_pixels)) {
            return 1;
        }
        
        frames_data.push_back(frame_pixels);
        std::cout << "[DEBUG] 📦 Extracted frame 1/1 with RGBA data\n";
    }
    
    // Get output format
    std::string mode;
    std::cout << "\nChoose format (HMIC / HMIC7): ";
    std::getline(std::cin, mode);
    std::transform(mode.begin(), mode.end(), mode.begin(), ::toupper);
    
    // 🧠 BUILD PER-FRAME PIXEL DATA
    std::cout << "\n[DEBUG] 🔥 Building per-frame RGBA pixel data with ALL " 
              << std::thread::hardware_concurrency() << " CORES...\n";
    
    std::vector<std::map<RGBA, std::vector<Command>>> frame_commands(n_frames);
    int num_threads = std::thread::hardware_concurrency();
    
    for (int frame_idx = 0; frame_idx < n_frames; frame_idx++) {
        std::cout << "\n[DEBUG] 🎨 Processing frame " << (frame_idx + 1) << "/" << n_frames 
                  << " with " << num_threads << " threads...\n";
        processed_rows = 0;
        
        int rows_per_chunk = std::max(1, h / num_threads);
        std::vector<std::thread> threads;
        std::vector<std::map<RGBA, std::vector<Command>>> thread_results(num_threads);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int t = 0; t < num_threads; t++) {
            int start_row = t * rows_per_chunk;
            int end_row = (t == num_threads - 1) ? h : (t + 1) * rows_per_chunk;
            
            threads.emplace_back(process_frame_rows_parallel,
                               std::ref(frames_data[frame_idx]), w, h,
                               start_row, end_row, &thread_results[t]);
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "[DEBUG] ⚡ Frame processed in " << duration.count() << "ms DEMOLISHED 🔥\n";
        
        // Merge thread results
        for (const auto& result : thread_results) {
            for (const auto& [color, cmds] : result) {
                frame_commands[frame_idx][color].insert(
                    frame_commands[frame_idx][color].end(),
                    cmds.begin(), cmds.end()
                );
            }
        }
    }
    
    // 🚀 TEMPORAL OPTIMIZATION
    std::cout << "\n[DEBUG] 🚀 Optimizing with temporal compression...\n";
    
    std::vector<std::set<Command>> merged_commands(n_frames);
    std::map<std::string, std::map<RGBA, std::vector<std::string>>> temporal_commands;
    
    for (int frame_idx = 0; frame_idx < n_frames - 1; frame_idx++) {
        for (const auto& [color, cmd_list] : frame_commands[frame_idx]) {
            for (const auto& cmd_data : cmd_list) {
                if (merged_commands[frame_idx].count(cmd_data)) continue;
                
                std::vector<int> consecutive_frames = {frame_idx + 1};
                
                for (int next_frame_idx = frame_idx + 1; next_frame_idx < n_frames; next_frame_idx++) {
                    bool match_found = false;
                    
                    if (frame_commands[next_frame_idx].count(color)) {
                        for (const auto& next_cmd_data : frame_commands[next_frame_idx][color]) {
                            if (next_cmd_data.x == cmd_data.x && 
                                next_cmd_data.end_x == cmd_data.end_x && 
                                next_cmd_data.y == cmd_data.y) {
                                
                                if (!merged_commands[next_frame_idx].count(next_cmd_data)) {
                                    consecutive_frames.push_back(next_frame_idx + 1);
                                    merged_commands[next_frame_idx].insert(next_cmd_data);
                                    match_found = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (!match_found) break;
                }
                
                if (consecutive_frames.size() > 1) {
                    std::string frame_range_str = frames_to_range_string(consecutive_frames);
                    temporal_commands[frame_range_str][color].push_back(cmd_data.cmd);
                    merged_commands[frame_idx].insert(cmd_data);
                }
            }
        }
        
        if ((frame_idx + 1) % 10 == 0) {
            std::cout << "[DEBUG] 🎯 Temporal optimization: " << (frame_idx + 1) << "/" 
                      << n_frames << " frames processed\n";
        }
    }
    
    std::cout << "[DEBUG] ✅ Created " << temporal_commands.size() 
              << " temporal command groups\n";
    
    // 🧾 BUILD HMIC TEXT DATA WITH RGBA!!
    std::cout << "\n[DEBUG] 📝 Building HMIC data structure with RGBA...\n";
    std::stringstream data;
    data << "info{\n";
    data << "DISPLAY=" << w << "X" << h << "\n";
    data << "FPS=" << fps << "\n";
    data << "F=" << n_frames << "\n";
    data << "LOOP=" << (loop ? "Y" : "N") << "\n";
    data << "}\n\n";
    
    // 🔥 Write temporal blocks first
    std::cout << "[DEBUG] 🎯 Writing temporal multi-frame blocks with RGBA...\n";
    for (const auto& [frame_range_str, color_commands] : temporal_commands) {
        data << "F" << frame_range_str << "{\n";
        for (const auto& [color, cmds] : color_commands) {
            // RGBA FORMAT NOW!! 🎨💎
            data << "  rgba(" << (int)color.r << "," << (int)color.g << "," 
                 << (int)color.b << "," << (int)color.a << "){\n";
            for (const auto& cmd : cmds) {
                data << "    " << cmd << "\n";
            }
            data << "  }\n";
        }
        data << "}\n";
    }
    
    // 🌈 Write individual frame blocks
    std::cout << "[DEBUG] 🎨 Writing individual frame blocks with RGBA...\n";
    for (int frame_idx = 0; frame_idx < n_frames; frame_idx++) {
        int frame_num = frame_idx + 1;
        std::stringstream frame_data;
        frame_data << "F" << frame_num << "{\n";
        bool has_content = false;
        
        for (const auto& [color, cmd_list] : frame_commands[frame_idx]) {
            bool color_written = false;
            
            for (const auto& cmd_data : cmd_list) {
                if (!merged_commands[frame_idx].count(cmd_data)) {
                    if (!color_written) {
                        has_content = true;
                        // RGBA HERE TOO!! 💎
                        frame_data << "  rgba(" << (int)color.r << "," << (int)color.g << "," 
                                   << (int)color.b << "," << (int)color.a << "){\n";
                        color_written = true;
                    }
                    frame_data << "    " << cmd_data.cmd << "\n";
                }
            }
            
            if (color_written) {
                frame_data << "  }\n";
            }
        }
        
        frame_data << "}\n";
        
        if (has_content) {
            data << frame_data.str();
        }
        
        if ((frame_idx + 1) % 10 == 0) {
            std::cout << "[DEBUG] ✅ Wrote frames 1-" << (frame_idx + 1) << "\n";
        }
    }
    
    std::string text_data = data.str();
    
    // 🚀 OUTPUT
    std::string base_name = fs::path(img_path).stem().string();
    
    std::cout << "\n[DEBUG] 💾 Writing output file...\n";
    
    if (mode == "HMIC") {
        std::string out_file = base_name + ".hmic";
        std::ofstream file(out_file);
        file << text_data;
        file.close();
        std::cout << "\n✅ HMIC file created successfully — " << out_file << " blessed with RGBA 💚\n";
    } else if (mode == "HMIC7") {
        std::string out_file = base_name + ".hmic7";
        
        size_t compressed_size = ZSTD_compressBound(text_data.size());
        std::vector<char> compressed_data(compressed_size);
        
        size_t actual_size = ZSTD_compress(compressed_data.data(), compressed_size,
                                          text_data.c_str(), text_data.size(), 19);
        
        if (ZSTD_isError(actual_size)) {
            std::cerr << "❌ Compression error: " << ZSTD_getErrorName(actual_size) << "\n";
            return 1;
        }
        
        std::ofstream file(out_file, std::ios::binary);
        file.write(compressed_data.data(), actual_size);
        file.close();
        
        std::cout << "\n🌀 HMIC7 file created — Zstd absolutely DEVOURED " << out_file 
                  << " (with RGBA!) no crumbs left 💾🔥\n";
    } else {
        std::cerr << "❌ invalid format, conversion canceled 😭\n";
        return 1;
    }
    
    // 🧮 File size flex
    std::cout << "\n📊 ═══ FINAL STATS ═══ 📊\n";
    std::cout << "📁 Input format: ." << ext << " (UNIVERSALLY SUPPORTED ✨)\n";
    if (fs::exists(base_name + ".hmic")) {
        std::cout << "HMIC size: " << (fs::file_size(base_name + ".hmic") / 1024.0) << " KB\n";
    }
    if (fs::exists(base_name + ".hmic7")) {
        std::cout << "HMIC7 size: " << (fs::file_size(base_name + ".hmic7") / 1024.0) << " KB\n";
    }
    
    if (is_gif) {
        std::cout << "🎬 Animation info: " << n_frames << " frames @ " << fps 
                  << " FPS, Loop=" << (loop ? "YES" : "NO") << " 🔥\n";
    }
    
    // 🔥 VERIFICATION STATS
    int total_commands = 0;
    for (const auto& [frame_range, color_map] : temporal_commands) {
        for (const auto& [color, cmds] : color_map) {
            total_commands += cmds.size();
        }
    }
    
    for (int frame_idx = 0; frame_idx < n_frames; frame_idx++) {
        for (const auto& [color, cmd_list] : frame_commands[frame_idx]) {
            for (const auto& cmd_data : cmd_list) {
                if (!merged_commands[frame_idx].count(cmd_data)) {
                    total_commands++;
                }
            }
        }
    }
    
    std::cout << "📊 Total commands generated: " << total_commands << "\n";
    std::cout << "📊 Image dimensions: " << w << "x" << h << " = " << (w * h) << " pixels per frame\n";
    std::cout << "📊 Total frames: " << n_frames << "\n";
    std::cout << "📊 Threads used: " << num_threads << " (FULL CORE BEAST MODE 💪)\n";
    std::cout << "\n💥 UNIVERSAL FORMAT CONVERSION COMPLETE 💥\n";
    std::cout << "🎉 ALL IMAGE FORMATS SUPPORTED WITH FULL RGBA!! 🎉\n";
    std::cout << "✨ JPG | PNG | BMP | TGA | PSD | GIF | HDR | PIC | PNM | WEBP ✨\n";
    std::cout << "🌐 WEBP powered by Google's libwebp - NO LIMITS!! 🌐\n";
    
    return 0;
}