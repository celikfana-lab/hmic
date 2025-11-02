#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <iomanip>
#include <chrono>

// 🌐 ENABLE WEBP SUPPORT!!
#define STBI_SUPPORT_WEBP
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// 🚀 LIBWEBP FOR SUPERIOR WEBP DECODING!!
#include <webp/decode.h>

// 🎬 FFMPEG FOR MP4/VIDEO SUPPORT!! 🎬
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <zstd.h>

namespace fs = std::filesystem;

std::mutex file_mutex;
std::atomic<int> processed_frames{0};
std::atomic<bool> progress_running{true};

const int MAX_THREADS = 4; // CHILL MODE - only use 4 threads max
const size_t MAX_MEMORY_MB = 512; // Don't use more than 512MB for buffers

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

// 🎯 CLEAN PROGRESS BAR!!
void show_progress_bar(int total_frames) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (progress_running) {
        int current = processed_frames.load();
        float percent = (float)current / total_frames * 100.0f;
        int bar_width = 50;
        int pos = bar_width * current / total_frames;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        // Calculate ETA
        int eta = (current > 0) ? (elapsed * (total_frames - current) / current) : 0;
        
        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "█";
            else if (i == pos) std::cout << "▶";
            else std::cout << "░";
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << percent << "% "
                  << "(" << current << "/" << total_frames << ") "
                  << elapsed << "s elapsed, ~" << eta << "s remaining" << std::flush;
        
        if (current >= total_frames) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\n";
}

std::string get_file_extension(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) return "";
    std::string ext = path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// 🎬 MEMORY-EFFICIENT VIDEO DECODER - PROCESSES ONE FRAME AT A TIME!!
class VideoStreamDecoder {
public:
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    int video_stream_idx = -1;
    int width = 0;
    int height = 0;
    int fps = 30;
    int total_frames = 0;
    
    AVFrame* frame = nullptr;
    AVFrame* frame_rgba = nullptr;
    AVPacket* packet = nullptr;
    
    bool open(const std::string& path) {
        if (avformat_open_input(&format_ctx, path.c_str(), nullptr, nullptr) < 0) {
            return false;
        }
        
        if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
            avformat_close_input(&format_ctx);
            return false;
        }
        
        AVCodecParameters* codec_params = nullptr;
        const AVCodec* codec = nullptr;
        
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_idx = i;
                codec_params = format_ctx->streams[i]->codecpar;
                codec = avcodec_find_decoder(codec_params->codec_id);
                break;
            }
        }
        
        if (video_stream_idx == -1 || !codec) {
            avformat_close_input(&format_ctx);
            return false;
        }
        
        codec_ctx = avcodec_alloc_context3(codec);
        if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
            avformat_close_input(&format_ctx);
            return false;
        }
        
        // Single thread for memory efficiency
        codec_ctx->thread_count = 1;
        
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
            avcodec_free_context(&codec_ctx);
            avformat_close_input(&format_ctx);
            return false;
        }
        
        width = codec_ctx->width;
        height = codec_ctx->height;
        
        AVRational frame_rate = format_ctx->streams[video_stream_idx]->avg_frame_rate;
        fps = (frame_rate.num && frame_rate.den) ? frame_rate.num / frame_rate.den : 30;
        
        total_frames = format_ctx->streams[video_stream_idx]->nb_frames;
        if (total_frames <= 0) {
            total_frames = (int)(format_ctx->streams[video_stream_idx]->duration * 
                                 av_q2d(format_ctx->streams[video_stream_idx]->time_base) * fps);
        }
        
        // Allocate reusable frames
        frame = av_frame_alloc();
        frame_rgba = av_frame_alloc();
        frame_rgba->format = AV_PIX_FMT_RGBA;
        frame_rgba->width = width;
        frame_rgba->height = height;
        av_frame_get_buffer(frame_rgba, 0);
        
        packet = av_packet_alloc();
        
        sws_ctx = sws_getContext(
            width, height, codec_ctx->pix_fmt,
            width, height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        return true;
    }
    
    // Returns true if frame decoded, false if done
    bool decode_next_frame(std::vector<RGBA>& pixels) {
        while (true) {
            int ret = av_read_frame(format_ctx, packet);
            
            if (ret < 0) return false; // End of video
            
            if (packet->stream_index == video_stream_idx) {
                if (avcodec_send_packet(codec_ctx, packet) >= 0) {
                    if (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                        sws_scale(sws_ctx, frame->data, frame->linesize, 0, height,
                                 frame_rgba->data, frame_rgba->linesize);
                        
                        // Copy to output pixels
                        pixels.resize(width * height);
                        uint8_t* src = frame_rgba->data[0];
                        int stride = frame_rgba->linesize[0];
                        
                        for (int y = 0; y < height; y++) {
                            for (int x = 0; x < width; x++) {
                                int idx = y * width + x;
                                int src_idx = y * stride + x * 4;
                                pixels[idx] = {
                                    src[src_idx],
                                    src[src_idx + 1],
                                    src[src_idx + 2],
                                    src[src_idx + 3]
                                };
                            }
                        }
                        
                        av_frame_unref(frame);
                        av_packet_unref(packet);
                        return true;
                    }
                }
            }
            av_packet_unref(packet);
        }
    }
    
    ~VideoStreamDecoder() {
        if (frame) av_frame_free(&frame);
        if (frame_rgba) av_frame_free(&frame_rgba);
        if (packet) av_packet_free(&packet);
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (codec_ctx) avcodec_free_context(&codec_ctx);
        if (format_ctx) avformat_close_input(&format_ctx);
    }
};

// Process frame with RLE compression
void process_frame(const std::vector<RGBA>& pixels, int w, int h, 
                   int frame_idx, std::ofstream& output) {
    std::map<RGBA, std::vector<std::string>> frame_commands;
    
    for (int y = 0; y < h; y++) {
        int x = 0;
        while (x < w) {
            RGBA pixel_color = pixels[y * w + x];
            
            // Run-length encoding
            int run_length = 1;
            while (x + run_length < w && pixels[y * w + x + run_length] == pixel_color) {
                run_length++;
            }
            
            std::string cmd;
            if (run_length == 1) {
                cmd = "P=" + std::to_string(x + 1) + "x" + std::to_string(y + 1);
            } else {
                int end_x = x + run_length - 1;
                cmd = "PL=" + std::to_string(x + 1) + "x" + std::to_string(y + 1) + 
                      "-" + std::to_string(end_x + 1) + "x" + std::to_string(y + 1);
            }
            
            frame_commands[pixel_color].push_back(cmd);
            x += run_length;
        }
    }
    
    // Write frame data
    std::stringstream frame_output;
    frame_output << "F" << frame_idx << "{\n";
    
    for (const auto& [color, cmd_list] : frame_commands) {
        frame_output << "  rgba(" << (int)color.r << "," << (int)color.g << "," 
                     << (int)color.b << "," << (int)color.a << "){\n";
        
        for (const auto& cmd : cmd_list) {
            frame_output << "    " << cmd << "\n";
        }
        
        frame_output << "  }\n";
    }
    
    frame_output << "}\n";
    
    output << frame_output.str();
}

bool load_webp_image(const std::string& path, int& w, int& h, std::vector<RGBA>& pixels) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return false;
    file.close();
    
    uint8_t* decoded = WebPDecodeRGBA(buffer.data(), buffer.size(), &w, &h);
    if (!decoded) return false;
    
    pixels.resize(w * h);
    for (int i = 0; i < w * h; i++) {
        pixels[i] = {decoded[i*4], decoded[i*4+1], decoded[i*4+2], decoded[i*4+3]};
    }
    
    WebPFree(decoded);
    return true;
}

bool load_universal_image(const std::string& path, int& w, int& h, std::vector<RGBA>& pixels) {
    std::string ext = get_file_extension(path);
    
    if (ext == "webp") {
        return load_webp_image(path, w, h, pixels);
    }
    
    int channels;
    unsigned char* img_data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!img_data) return false;
    
    pixels.resize(w * h);
    for (int i = 0; i < w * h; i++) {
        pixels[i] = {img_data[i*4], img_data[i*4+1], img_data[i*4+2], img_data[i*4+3]};
    }
    
    stbi_image_free(img_data);
    return true;
}

int main() {
    std::cout << "🎬 RAM-FRIENDLY VIDEO CONVERTER 🎬\n";
    std::cout << "💚 Memory-efficient single-pass processing! 💚\n\n";
    
    std::string img_path;
    std::cout << "Enter media file path: ";
    std::getline(std::cin, img_path);
    
    if (!fs::exists(img_path)) {
        std::cerr << "❌ File not found\n";
        return 1;
    }
    
    std::string ext = get_file_extension(img_path);
    bool is_video = (ext == "mp4" || ext == "avi" || ext == "mov" || 
                     ext == "mkv" || ext == "webm" || ext == "flv");
    
    std::string mode;
    std::cout << "Choose format (HMIC / HMIC7): ";
    std::getline(std::cin, mode);
    std::transform(mode.begin(), mode.end(), mode.begin(), ::toupper);
    
    std::string base_name = fs::path(img_path).stem().string();
    std::string temp_file = base_name + "_temp.hmic";
    std::ofstream output(temp_file);
    
    int w = 0, h = 0, n_frames = 1, fps = 1;
    
    if (is_video) {
        std::cout << "\n🎬 VIDEO MODE - Memory-efficient processing! 🎬\n";
        
        VideoStreamDecoder decoder;
        if (!decoder.open(img_path)) {
            std::cerr << "❌ Failed to open video\n";
            return 1;
        }
        
        w = decoder.width;
        h = decoder.height;
        fps = decoder.fps;
        n_frames = decoder.total_frames;
        
        std::cout << "📊 VIDEO: " << w << "x" << h << " @ " << fps << " FPS\n";
        std::cout << "🎞️ TOTAL FRAMES: " << n_frames << "\n";
        size_t frame_size_mb = (w * h * 4) / (1024 * 1024);
        std::cout << "💾 Memory per frame: ~" << frame_size_mb << " MB\n\n";
        
        output << "info{\n";
        output << "DISPLAY=" << w << "X" << h << "\n";
        output << "FPS=" << fps << "\n";
        output << "F=" << n_frames << "\n";
        output << "LOOP=Y\n";
        output << "}\n\n";
        
        // Start progress bar in separate thread
        progress_running = true;
        std::thread progress_thread(show_progress_bar, n_frames);
        
        // Process frames ONE AT A TIME - no queue, no memory explosion!
        std::vector<RGBA> pixels;
        int frame_count = 1;
        
        while (decoder.decode_next_frame(pixels)) {
            process_frame(pixels, w, h, frame_count, output);
            processed_frames++;
            frame_count++;
            
            // Clear pixel data to free memory immediately
            pixels.clear();
            pixels.shrink_to_fit();
        }
        
        progress_running = false;
        progress_thread.join();
        
    } else {
        std::cout << "\n🖼️ IMAGE MODE! 🖼️\n";
        
        std::vector<RGBA> pixels;
        if (!load_universal_image(img_path, w, h, pixels)) {
            std::cerr << "❌ Failed to load image\n";
            return 1;
        }
        
        std::cout << "📊 IMAGE: " << w << "x" << h << "\n\n";
        
        output << "info{\n";
        output << "DISPLAY=" << w << "X" << h << "\n";
        output << "FPS=1\n";
        output << "F=1\n";
        output << "LOOP=N\n";
        output << "}\n\n";
        
        process_frame(pixels, w, h, 1, output);
        processed_frames = 1;
    }
    
    output.close();
    
    if (mode == "HMIC7") {
        std::cout << "\n🗜️ Compressing with ZSTD...\n";
        
        std::ifstream temp_in(temp_file, std::ios::binary);
        std::string text_data((std::istreambuf_iterator<char>(temp_in)),
                              std::istreambuf_iterator<char>());
        temp_in.close();
        
        size_t compressed_bound = ZSTD_compressBound(text_data.size());
        std::vector<char> compressed(compressed_bound);
        size_t size = ZSTD_compress(compressed.data(), compressed.size(),
                                    text_data.c_str(), text_data.size(), 3);
        
        std::ofstream final_out(base_name + ".hmic7", std::ios::binary);
        final_out.write(compressed.data(), size);
        final_out.close();
        
        fs::remove(temp_file);
        
        std::cout << "✅ HMIC7 CREATED! 💾\n";
        std::cout << "📉 COMPRESSED: " << text_data.size() << " → " << size << " bytes\n";
    } else {
        fs::rename(temp_file, base_name + ".hmic");
        std::cout << "\n✅ HMIC CREATED! 💚\n";
    }
    
    std::cout << "\n💥 CONVERSION COMPLETE! 💥\n";
    std::cout << "🎉 " << n_frames << " frames @ " << fps << " FPS 🎉\n";
    
    return 0;
}