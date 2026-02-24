#include "pch.h"

#include "Logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <mutex>

namespace com_khelai_zcamnative
{
    std::shared_ptr<spdlog::logger> Logger::s_sharedLogger;
    static std::mutex g_loggerMutex;

    Logger::Logger(const std::string& logFilePath)
    {
        InitSharedLogger(logFilePath);
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
}