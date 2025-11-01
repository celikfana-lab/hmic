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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <zstd.h>

namespace fs = std::filesystem;

std::mutex cout_mutex;
std::atomic<int> processed_rows{0};

struct RGB {
    uint8_t r, g, b;
    
    bool operator<(const RGB& other) const {
        if (r != other.r) return r < other.r;
        if (g != other.g) return g < other.g;
        return b < other.b;
    }
    
    bool operator==(const RGB& other) const {
        return r == other.r && g == other.g && b == other.b;
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

// 🚀 PROCESS ROWS IN PARALLEL - OPTIMIZED FOR YOUR 56 CORES!!
void process_frame_rows_parallel(
    const std::vector<RGB>& frame_pixels,
    int w, int h,
    int start_row, int end_row,
    std::map<RGB, std::vector<Command>>* local_commands
) {
    for (int y = start_row; y < end_row; y++) {
        int x = 0;
        while (x < w) {
            RGB pixel_color = frame_pixels[y * w + x];
            
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
    std::cout << "🔥🔥🔥 C++ TURBO MODE — " << std::thread::hardware_concurrency() 
              << " CORES DETECTED 🔥🔥🔥\n";
    std::cout << "💪 56 THREADS + 64GB ECC RAM = ABSOLUTELY UNSTOPPABLE 💪\n\n";
    
    // 🩵 Load the sacred image/gif
    std::string img_path;
    std::cout << "Enter image file path: ";
    std::getline(std::cin, img_path);
    
    // Check if it's a GIF
    bool is_gif = (img_path.substr(img_path.find_last_of('.') + 1) == "gif");
    
    int w, h, n_frames = 1, fps = 1;
    bool loop = true;
    std::vector<std::vector<RGB>> frames_data;
    
    if (is_gif) {
        std::cout << "\n🎬 GIF MODE ACTIVATED - USING STB_IMAGE (SAME AS PIL!) 🎬\n";
        
        // 🎬 LOAD ANIMATED GIF USING STB_IMAGE (HANDLES DISPOSAL MODES AUTOMATICALLY!)
        FILE* f = fopen(img_path.c_str(), "rb");
        if (!f) {
            std::cerr << "❌ Failed to open GIF file\n";
            return 1;
        }
        fclose(f);
        
        int channels;
        int* delays = nullptr;
        int z = 0; // number of frames
        
        // Load all frames with stbi_load_gif
        unsigned char* img_data = stbi_load_gif_from_memory(nullptr, 0, &delays, &w, &h, &z, &channels, 3);
        
        // If that fails, try regular load first
        if (!img_data) {
            // Try loading file into memory first
            std::ifstream file(img_path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                std::cerr << "❌ Failed to open GIF file\n";
                return 1;
            }
            
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            std::vector<unsigned char> buffer(size);
            if (!file.read((char*)buffer.data(), size)) {
                std::cerr << "❌ Failed to read GIF file\n";
                return 1;
            }
            file.close();
            
            // Now load GIF from memory
            img_data = stbi_load_gif_from_memory(buffer.data(), buffer.size(), 
                                                &delays, &w, &h, &z, &channels, 3);
        }
        
        if (!img_data) {
            std::cerr << "❌ Failed to load GIF: " << stbi_failure_reason() << "\n";
            std::cerr << "💡 STB might not support this GIF, try converting to PNG sequence\n";
            return 1;
        }
        
        n_frames = z;
        
        std::cout << "📊 GIF INFO: " << w << "x" << h << " @ " << n_frames << " frames\n";
        
        // Calculate FPS from delays (delays are in milliseconds)
        if (delays && delays[0] > 0) {
            fps = std::max(1, 1000 / delays[0]);
            std::cout << "🎬 Frame delay: " << delays[0] << "ms → " << fps << " FPS\n";
        } else {
            fps = 10; // Default to 10 FPS
            std::cout << "⚠️ No delay info, defaulting to 10 FPS\n";
        }
        
        std::cout << "🎬 ANIMATED GIF: " << n_frames << " frames @ " << fps << " FPS 🔥\n\n";
        
        // 🎨 EXTRACT ALL FRAMES (STB HAS ALREADY COMPOSED THEM PROPERLY!)
        for (int frame_idx = 0; frame_idx < n_frames; frame_idx++) {
            std::vector<RGB> frame_pixels(w * h);
            
            // STB stores frames sequentially in memory
            int frame_offset = frame_idx * w * h * 3;
            
            for (int i = 0; i < w * h; i++) {
                frame_pixels[i] = {
                    img_data[frame_offset + i * 3],
                    img_data[frame_offset + i * 3 + 1],
                    img_data[frame_offset + i * 3 + 2]
                };
            }
            
            frames_data.push_back(frame_pixels);
            std::cout << "[DEBUG] 📦 Extracted frame " << (frame_idx + 1) << "/" << n_frames 
                      << " (STB handled all disposal modes!) ✅\n";
        }
        
        // Free STB memory
        stbi_image_free(img_data);
        if (delays) free(delays);
        
        // 🔍 DIAGNOSTIC: Save first frame as PNG
        std::string debug_png = "debug_frame1_stb.png";
        std::vector<uint8_t> png_data(w * h * 3);
        for (int i = 0; i < w * h; i++) {
            png_data[i * 3] = frames_data[0][i].r;
            png_data[i * 3 + 1] = frames_data[0][i].g;
            png_data[i * 3 + 2] = frames_data[0][i].b;
        }
        stbi_write_png(debug_png.c_str(), w, h, 3, png_data.data(), w * 3);
        std::cout << "🔍 SAVED " << debug_png << " - CHECK IF THIS LOOKS CORRECT!! 💎\n\n";
        
    } else {
        // 📸 LOAD SINGLE IMAGE
        std::cout << "\n📸 Single frame image detected — classic mode 💚\n";
        
        int channels;
        unsigned char* img_data = stbi_load(img_path.c_str(), &w, &h, &channels, 3);
        
        if (!img_data) {
            std::cerr << "❌ Failed to load image: " << stbi_failure_reason() << "\n";
            return 1;
        }
        
        std::vector<RGB> frame_pixels(w * h);
        for (int i = 0; i < w * h; i++) {
            frame_pixels[i] = {img_data[i * 3], img_data[i * 3 + 1], img_data[i * 3 + 2]};
        }
        
        frames_data.push_back(frame_pixels);
        stbi_image_free(img_data);
        
        std::cout << "[DEBUG] 📦 Extracted frame 1/1\n";
    }
    
    // Get output format
    std::string mode;
    std::cout << "\nChoose format (HMIC / HMIC7): ";
    std::getline(std::cin, mode);
    std::transform(mode.begin(), mode.end(), mode.begin(), ::toupper);
    
    // 🧠 BUILD PER-FRAME PIXEL DATA
    std::cout << "\n[DEBUG] 🔥 Building per-frame pixel data with ALL " 
              << std::thread::hardware_concurrency() << " CORES...\n";
    
    std::vector<std::map<RGB, std::vector<Command>>> frame_commands(n_frames);
    int num_threads = std::thread::hardware_concurrency();
    
    for (int frame_idx = 0; frame_idx < n_frames; frame_idx++) {
        std::cout << "\n[DEBUG] 🎨 Processing frame " << (frame_idx + 1) << "/" << n_frames 
                  << " with " << num_threads << " threads...\n";
        processed_rows = 0;
        
        int rows_per_chunk = std::max(1, h / num_threads);
        std::vector<std::thread> threads;
        std::vector<std::map<RGB, std::vector<Command>>> thread_results(num_threads);
        
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
    std::map<std::string, std::map<RGB, std::vector<std::string>>> temporal_commands;
    
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
    
    // 🧾 BUILD HMIC TEXT DATA
    std::cout << "\n[DEBUG] 📝 Building HMIC data structure...\n";
    std::stringstream data;
    data << "info{\n";
    data << "DISPLAY=" << w << "X" << h << "\n";
    data << "FPS=" << fps << "\n";
    data << "F=" << n_frames << "\n";
    data << "LOOP=" << (loop ? "Y" : "N") << "\n";
    data << "}\n\n";
    
    // 🔥 Write temporal blocks first
    std::cout << "[DEBUG] 🎯 Writing temporal multi-frame blocks...\n";
    for (const auto& [frame_range_str, color_commands] : temporal_commands) {
        data << "F" << frame_range_str << "{\n";
        for (const auto& [color, cmds] : color_commands) {
            data << "  rgb(" << (int)color.r << "," << (int)color.g << "," << (int)color.b << "){\n";
            for (const auto& cmd : cmds) {
                data << "    " << cmd << "\n";
            }
            data << "  }\n";
        }
        data << "}\n";
    }
    
    // 🌈 Write individual frame blocks
    std::cout << "[DEBUG] 🎨 Writing individual frame blocks...\n";
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
                        frame_data << "  rgb(" << (int)color.r << "," << (int)color.g << "," << (int)color.b << "){\n";
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
        std::cout << "\n✅ HMIC file created successfully — " << out_file << " blessed 💚\n";
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
                  << " no crumbs left 💾🔥\n";
    } else {
        std::cerr << "❌ invalid format, Miku has left the chat 😭\n";
        return 1;
    }
    
    // 🧮 File size flex
    std::cout << "\n📊 ═══ FINAL STATS ═══ 📊\n";
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
    std::cout << "📊 Threads used: " << num_threads << " (YOUR 56 CORE BEAST MODE 💪)\n";
    std::cout << "\n💥 Conversion complete — behold the pure RGB chaos, alpha banished 💥\n";
    std::cout << "🎉 USING STB_IMAGE JUST LIKE PYTHON PIL — NO MORE GLITCHES!! 🎉\n";
    
    return 0;
}