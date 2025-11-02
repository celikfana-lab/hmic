#include "hmicx.h"
#include <zstd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <thread>
#include <future>
#include <mutex>

using namespace std;
using namespace HMICX;

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
    uint8_t r, g, b, a;
};

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
    } else if (str.find("rgba(") == 0) {
        int r, g, b, a;
        if (sscanf(str.c_str(), "rgba(%d,%d,%d,%d)", &r, &g, &b, &a) == 4) {
            c.r = (uint8_t)r; c.g = (uint8_t)g; c.b = (uint8_t)b; c.a = (uint8_t)a;
        }
    } else if (str.find("rgb(") == 0) {
        int r, g, b;
        if (sscanf(str.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3) {
            c.r = (uint8_t)r; c.g = (uint8_t)g; c.b = (uint8_t)b; c.a = 255;
        }
    }
    return c;
}

void compressToHMICP7(const string& hmicpPath, const string& hmicp7Path) {
    cout << "[DEBUG] 🗜️ Compressing to HMICP7 (multi-threaded mode)..." << endl;
    ifstream in(hmicpPath, ios::binary | ios::ate);
    if (!in.is_open()) throw runtime_error("Failed to open HMICP file: " + hmicpPath);

    streamsize size = in.tellg();
    in.seekg(0, ios::beg);
    vector<char> buffer(size);
    in.read(buffer.data(), size);
    in.close();

    size_t bound = ZSTD_compressBound(size);
    vector<char> compressed(bound);

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 19);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, std::thread::hardware_concurrency());

    size_t compressedSize = ZSTD_compress2(cctx, compressed.data(), bound, buffer.data(), size);
    if (ZSTD_isError(compressedSize))
        throw runtime_error(string("Zstd compression failed: ") + ZSTD_getErrorName(compressedSize));

    ZSTD_freeCCtx(cctx);
    compressed.resize(compressedSize);

    ofstream out(hmicp7Path, ios::binary);
    out.write(compressed.data(), compressed.size());
    out.close();

    float ratio = (1.0f - (float)compressedSize / (float)size) * 100.0f;
    cout << "[DEBUG] 🔥 Compressed " << size << " → " << compressedSize
         << " bytes (" << ratio << "% reduction) using "
         << std::thread::hardware_concurrency() << " threads 💪" << endl;
}

void renderAndWriteHMICP(const string& outputPath, const HMICPHeader& header,
                         const vector<Command>& commands, int width, int height, int totalFrames) {
    cout << "[DEBUG] 🚀 Rendering in parallel mode with "
         << std::thread::hardware_concurrency() << " threads..." << endl;

    ofstream out(outputPath, ios::binary);
    if (!out.is_open()) throw runtime_error("Failed to create output file: " + outputPath);
    out.write(reinterpret_cast<const char*>(&header), sizeof(HMICPHeader));

    RGBA blackTransparent = {0, 0, 0, 0};
    vector<future<vector<RGBA>>> futures;
    mutex writeMutex;

    auto renderFrame = [&](int f) -> vector<RGBA> {
        vector<RGBA> frame(width * height, blackTransparent);
        for (const auto& cmd : commands) {
            if (f + 1 < cmd.start || f + 1 > cmd.end) continue;
            RGBA color = parseColor(cmd.color);
            for (const auto& px : cmd.pixels) {
                int x = px.x - 1, y = px.y - 1;
                if (x < 0 || x >= width || y < 0 || y >= height) continue;
                RGBA& bg = frame[y * width + x];
                if (color.a == 255) bg = color;
                else if (color.a > 0) {
                    float a = color.a / 255.0f;
                    bg.r = (uint8_t)(color.r * a + bg.r * (1 - a));
                    bg.g = (uint8_t)(color.g * a + bg.g * (1 - a));
                    bg.b = (uint8_t)(color.b * a + bg.b * (1 - a));
                    bg.a = max(bg.a, color.a);
                }
            }
        }
        return frame;
    };

    const int threadCount = std::thread::hardware_concurrency();
    for (int f = 0; f < totalFrames; f++) {
        if ((int)futures.size() >= threadCount) {
            for (auto& fut : futures) {
                vector<RGBA> frame = fut.get();
                lock_guard<mutex> lock(writeMutex);
                out.write(reinterpret_cast<const char*>(frame.data()), frame.size() * sizeof(RGBA));
            }
            futures.clear();
        }
        futures.push_back(async(std::launch::async, renderFrame, f));
    }

    for (auto& fut : futures) {
        vector<RGBA> frame = fut.get();
        out.write(reinterpret_cast<const char*>(frame.data()), frame.size() * sizeof(RGBA));
    }

    out.close();
    cout << "[DEBUG] 💾 HMICP writing complete with full CPU utilization 💥" << endl;
}

int main() {
    cout << "🔥🔥🔥 HMIC → HMICP STREAMING CONVERTER 🔥🔥🔥" << endl;
    cout << "MULTI-CORE CHAOS EDITION 💾💪" << endl;

    string inputPath;
    cout << "Enter HMIC/HMIC7 file path: ";
    getline(cin, inputPath);

    try {
        bool isCompressed = false;
        if (inputPath.size() >= 6) {
            string ext = inputPath.substr(inputPath.size() - 6);
            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            isCompressed = (ext == ".hmic7");
        }

        string parsePath = inputPath;
        string tempFile = ".temp_decompressed.hmic";

        if (isCompressed) {
            cout << "[DEBUG] 🌀 Decompressing input..." << endl;
            ifstream in(inputPath, ios::binary | ios::ate);
            if (!in.is_open()) throw runtime_error("Failed to open HMIC7 file");
            streamsize size = in.tellg();
            in.seekg(0, ios::beg);
            vector<char> compressed(size);
            in.read(compressed.data(), size);
            in.close();

            unsigned long long decompSize = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
            vector<char> decompressed(decompSize);

            size_t result = ZSTD_decompress(decompressed.data(), decompressed.size(), compressed.data(), compressed.size());
            if (ZSTD_isError(result))
                throw runtime_error(string("Decompression failed: ") + ZSTD_getErrorName(result));

            ofstream temp(tempFile, ios::binary);
            temp.write(decompressed.data(), decompressed.size());
            temp.close();
            parsePath = tempFile;
            cout << "[DEBUG] ✅ Decompressed successfully!" << endl;
        }

        Parser parser(parsePath);
        parser.parse();
        auto headerMap = parser.getHeader();
        auto commands = parser.getCommands();

        int width = 5, height = 5, fps = 2, totalFrames = 1;
        bool loop = true;

        for (const auto& [key, val] : headerMap) {
            string k = key;
            transform(k.begin(), k.end(), k.begin(), ::toupper);
            if (k == "DISPLAY") sscanf(val.c_str(), "%dx%d", &width, &height);
            else if (k == "FPS") fps = stoi(val);
            else if (k == "F") totalFrames = stoi(val);
            else if (k == "LOOP") loop = (val == "Y" || val == "y" || val == "1");
        }

        cout << "[DEBUG] 📊 Metadata: " << width << "x" << height
             << ", " << fps << " FPS, " << totalFrames
             << " frames, Loop=" << (loop ? "YES" : "NO") << endl;

        HMICPHeader hmicpHeader;
        hmicpHeader.width = width;
        hmicpHeader.height = height;
        hmicpHeader.fps = fps;
        hmicpHeader.totalFrames = totalFrames;
        hmicpHeader.loop = loop ? 1 : 0;

        string baseName = inputPath;
        if (isCompressed) baseName = baseName.substr(0, baseName.size() - 6);
        else baseName = baseName.substr(0, baseName.size() - 5);

        string hmicpPath = baseName + ".hmicp";
        string hmicp7Path = baseName + ".hmicp7";

        renderAndWriteHMICP(hmicpPath, hmicpHeader, commands, width, height, totalFrames);
        compressToHMICP7(hmicpPath, hmicp7Path);

        if (isCompressed) remove(tempFile.c_str());

        cout << "🎉 DONE BRO!! You got:\n - " << hmicpPath << "\n - " << hmicp7Path
             << "\nSTREAMING SUCCESS 💾🔥 FULL MULTICORE POWER UNLEASHED 💀💀💀" << endl;

    } catch (const exception& e) {
        cerr << "❌ ERROR: " << e.what() << endl;
        return 1;
    }

    return 0;
}
