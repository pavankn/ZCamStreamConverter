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

std::shared_ptr<spdlog::logger> Logger::s_sharedLogger;
static std::mutex g_loggerMutex;


std::string GetAppDataLogPath(const std::string& fileName) {
    char path[MAX_PATH];

    // Get C:\Users\<User>\AppData\Local
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path) == S_OK) {
        std::string fullPath = std::string(path) + "\\KhelAI" + "\\ZCamStreamConverter";

        // Create the subfolder if it doesn't exist
        // (CreateDirectoryA returns 0 if it fails or already exists)
        CreateDirectoryA(fullPath.c_str(), NULL);

        return fullPath + "\\" + fileName;
    }

    return fileName; // Fallback to current directory
}

Logger::Logger(const std::string& logFilePath)
{
    std::string fullLogPath = GetAppDataLogPath(logFilePath);
    InitSharedLogger(fullLogPath);
    _logger = s_sharedLogger;
}

void Logger::InitSharedLogger(const std::string& logFilePath)
{
    std::lock_guard<std::mutex> lock(g_loggerMutex);

    if (s_sharedLogger)
        return; // already initialized

    // Drop existing logger if name reused (safe guard)
    spdlog::drop("zcam");

    s_sharedLogger = spdlog::basic_logger_mt(
        "zcam",            // logger name (constant!)
        logFilePath,       // single log file
        true               // truncate on startup
    );

    s_sharedLogger->set_level(spdlog::level::debug);
    s_sharedLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
    s_sharedLogger->flush_on(spdlog::level::info);
}

void Logger::info(const std::string& msg) { _logger->info(msg); }
void Logger::warn(const std::string& msg) { _logger->warn(msg); }
void Logger::error(const std::string& msg) { _logger->error(msg); }
void Logger::debug(const std::string& msg) { _logger->debug(msg); }
