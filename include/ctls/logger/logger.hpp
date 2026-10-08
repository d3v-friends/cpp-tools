#pragma once

// platform 별 헤더 불러오기
#if defined(_WIN32)
#include "ctls/logger/windows.hpp"
#elif defined(__APPLE__)
#include "ctls/logger/mac_linux.hpp"
#elif defined(__linux__)
#include "ctls/logger/mac_linux.hpp"
#endif
