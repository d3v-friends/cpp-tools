#pragma once
#include <cassert>
#include <source_location>

#include "logger.hpp"


#ifdef NDEBUG
#define IASSERT(expr, ...) ((void)0)
#else
#define IASSERT(expr, ...)                                                               \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            __VA_OPT__(ILOG(::ctls::e_log_level::error, __VA_ARGS__);)                 \
            assert((false && #expr));                                                    \
        }                                                                                \
    } while (0)
#endif