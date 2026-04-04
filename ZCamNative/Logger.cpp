#include "Logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <mutex>
#include <filesystem>
#include <windows.h>
#include <shlobj.h>
#include <string>

#include <atlbase.h> // For CComPtr or manual CoTaskMemFree

#pragma comment(lib, "shell32.lib")



namespace com_khelai_zcamnative
{
    std::shared_ptr<spdlog::logger> Logger::s_logger = nullptr;
    std::mutex Logger::s_mutex;

    std::string GetAppDataLogPath(const std::string& fileName) {
        char path[MAX_PATH];

        // Get C:\Users\<User>\AppData\Local
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path) == S_OK) {
            std::string fullPath = std::string(path) + "\\KhelAI" + "\\Logs";

            // Create the subfolder if it doesn't exist
            // (CreateDirectoryA returns 0 if it fails or already exists)
            CreateDirectoryA(fullPath.c_str(), NULL);

            return fullPath + "\\" + fileName;
        }

        return fileName; // Fallback to current directory
    }    

    std::shared_ptr<spdlog::logger> Logger::Get() {
        return com_khelai_zcamnative::Logger::s_logger;
    }

    void Logger::Init(const std::string& logFilePath)
    {
        std::lock_guard<std::mutex> lock(s_mutex);

        std::string fullLogPath = GetAppDataLogPath(logFilePath);

        if (s_logger)
            return; // already initialized

        // Drop existing logger if name reused (safe guard)
        spdlog::drop("zcam");

        s_logger = spdlog::basic_logger_mt(
            "zcam",            // logger name (constant!)
            fullLogPath,       // single log file
            true               // truncate on startup
        );

        s_logger->set_level(spdlog::level::debug);
        s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        s_logger->flush_on(spdlog::level::info);
    }

    void Logger::info(const std::string& msg) { Logger::Get()->info(msg); }
    void Logger::warn(const std::string& msg) { Logger::Get()->warn(msg); }
    void Logger::error(const std::string& msg) { Logger::Get()->error(msg); }
    void Logger::debug(const std::string& msg) { Logger::Get()->debug(msg); }
}

