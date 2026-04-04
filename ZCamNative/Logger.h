#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <string>



namespace com_khelai_zcamnative
{
    class Logger
    {
    public:
        static std::shared_ptr<spdlog::logger> Get();
        static void Init(const std::string& logFilePath);

        void info(const std::string& msg);
        void warn(const std::string& msg);
        void error(const std::string& msg);
        void debug(const std::string& msg);

        template<typename... Args>
        static void info(const char* fmt, Args&&... args)
        {
            Get()->info(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warn(const char* fmt, Args&&... args)
        {
            Get()->warn(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(const char* fmt, Args&&... args)
        {
            Get()->error(fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(const char* fmt, Args&&... args)
        {
            Get()->debug(fmt, std::forward<Args>(args)...);
        }

    private:
        static std::shared_ptr<spdlog::logger> s_logger;
        static std::mutex s_mutex;
    };
}