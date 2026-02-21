#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

namespace Log {
    enum class Level { Info, Warn, Error };

    inline std::mutex mtx;
    inline std::optional<std::ofstream> logFile;

    inline void setFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx);
        std::ofstream f(path, std::ios::app);
        if (!f) {
            std::cerr << "[Error] Cannot open log file '" << path << "'\n";
            return;
        }
        logFile.emplace(std::move(f));
    }

    inline void raw(Level lvl, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        const char* tag = (lvl == Level::Info) ? "[Info]  " :
                         (lvl == Level::Warn) ? "[Warn]  " : "[Error] ";
        std::cout << tag << msg << '\n';
        if (logFile) (*logFile) << tag << msg << '\n';
    }

    inline void info (const std::string& msg) { raw(Level::Info,  msg); }
    inline void warn (const std::string& msg) { raw(Level::Warn,  msg); }
    inline void error(const std::string& msg) { raw(Level::Error, msg); }
}
