#pragma once

#include <Windows.h>
#include <afxwin.h>

#include <functional>

namespace logger
{

    using logger_t = std::function < void(CString) > ;

    void log(CString s, ...);

    void set(logger_t logger);
}