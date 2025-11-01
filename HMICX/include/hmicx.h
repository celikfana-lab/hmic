#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility>

namespace HMICX {

    struct Pixel {
        int x, y;
    };

    struct Command {
        int start, end;
        std::vector<Pixel> pixels;
        std::string color;
    };

    class Parser {
    private:
        std::string content;
        std::map<std::string, std::string> header;
        std::vector<Command> commands;
        
        // ⚡ UPDATED SIGNATURES FOR ZERO-COPY PARSING ⚡
        void parseHeader();
        void parseHeaderBody(const char* body, size_t len);  // ← CHANGED
        void parseFrames();
        void parseFrameBody(const char* body, size_t len, int start, int end);  // ← CHANGED
        std::vector<Pixel> parsePixels(const char* body, size_t len);  // ← CHANGED

    public:
        Parser(const std::string& filepath);
        void parse();
        std::map<std::string, std::string> getHeader() const;
        std::vector<Command> getCommands() const;
    };
}