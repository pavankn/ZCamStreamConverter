#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <string>

namespace com_khelai_zcamnative
{
    class Logger
    {
    public:
        explicit Logger(const std::string& logFilePath);

        void info(const std::string& msg);
        void warn(const std::string& msg);
        void error(const std::string& msg);
        void debug(const std::string& msg);

        template<typename... Args>
        void info(const char* fmt, Args&&... args)
        {
            _logger->info(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void warn(const char* fmt, Args&&... args)
        {
            _logger->warn(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void error(const char* fmt, Args&&... args)
        {
            _logger->error(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void debug(const char* fmt, Args&&... args)
        {
            _logger->debug(fmt, std::forward<Args>(args)...);
        }

    private:
        static std::shared_ptr<class spdlog::logger> s_sharedLogger;
        std::shared_ptr<class spdlog::logger> _logger;

        static void InitSharedLogger(const std::string& logFilePath);
    };
}