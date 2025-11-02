#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <cstddef>
#include <cctype>

namespace HMICX {

    // 📦 DATA STRUCTURES - KEEPING IT CLEAN
    struct Pixel {
        int x, y;
    };

    struct Command {
        int start, end;
        std::vector<Pixel> pixels;
        std::string color;
    };

    // 🎯 MAIN PARSER CLASS - THE STAR OF THE SHOW
    class Parser {
    private:
        std::string content;
        std::map<std::string, std::string> header;
        std::vector<Command> commands;
        
        // Core parsing methods (implementation in .cpp)
        void parseHeader();
        void parseHeaderBody(const char* body, size_t len);
        void parseFrames();
        void parseFrameBody(const char* body, size_t len, int start, int end);
        std::vector<Pixel> parsePixels(const char* body, size_t len);

    public:
        Parser(const std::string& filepath);
        void parse();
        std::map<std::string, std::string> getHeader() const;
        std::vector<Command> getCommands() const;
    };

    // ⚡ INLINE HELPER FUNCTIONS - ZERO-COPY OPTIMIZATION GO BRRRR ⚡
    // These are inline so they get embedded at call sites for MAXIMUM SPEED 🏎️
    // Defined in header = no duplicate symbol errors + compiler can optimize better
    
    // Fast case-insensitive prefix check without allocating strings
    inline bool fastStartsWith(const char* str, size_t len, const char* prefix, size_t prefixLen) {
        if (len < prefixLen) return false;
        for (size_t i = 0; i < prefixLen; i++) {
            if (std::tolower(static_cast<unsigned char>(str[i])) != 
                std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
        }
        return true;
    }

    // Extract number directly from char array without string conversion
    inline int fastExtractNumber(const char* str, size_t len, size_t& pos) {
        int num = 0;
        bool found = false;
        while (pos < len && std::isdigit(static_cast<unsigned char>(str[pos]))) {
            num = num * 10 + (str[pos] - '0');
            pos++;
            found = true;
        }
        return found ? num : -1;
    }

    // Find matching closing brace with single scan
    inline size_t findMatchingBrace(const char* s, size_t len, size_t start) {
        int depth = 0;
        for (size_t i = start; i < len; i++) {
            if (s[i] == '{') depth++;
            else if (s[i] == '}') {
                depth--;
                if (depth == 0) return i;
            }
        }
        return std::string::npos;
    }

    // Trim whitespace without creating new strings
    inline std::pair<const char*, size_t> fastTrim(const char* start, size_t len) {
        const char* end = start + len;
        while (start < end && std::isspace(static_cast<unsigned char>(*start))) start++;
        while (start < end && std::isspace(static_cast<unsigned char>(*(end - 1)))) end--;
        return {start, static_cast<size_t>(end - start)};
    }

}  // namespace HMICX