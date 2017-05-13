#include "stdafx.h"

#include "logger.h"

#include <mutex>

namespace logger
{
    namespace
    {
        static logger_t _log([] (CString) {});
        static std::mutex log_guard;
    }

    void log(CString s, ...)
    {
        std::lock_guard<std::mutex> guard(log_guard);
        va_list args;
        va_start(args, s);
        CString f; f.FormatV(s, args);
        _log(f);
        va_end(args);
    }

    void log(CString s)
    {
        std::lock_guard<std::mutex> guard(log_guard);
        _log(s);
    }

    void set(logger_t logger)
    {
        std::lock_guard<std::mutex> guard(log_guard);
        _log = logger;
    }
}