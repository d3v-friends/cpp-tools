#pragma once
#include <cassert>
#include <source_location>

#include "logger.hpp"

// deprecated
#ifdef NDEBUG
#define I_ASSERT(expr, str) ((void)0)
#define I_ASSERT_F(expr, fmt, ...) ((void)0)
#else
#define I_ASSERT_F(expr, fmt, ...)                                                       \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            ctls::printf_line(                                                           \
                std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__);        \
            assert((expr));                                                              \
        }                                                                                \
    } while (0)

#define I_ASSERT(expr, str)                                                              \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            ctls::print_line(std::source_location::current(), str);                      \
            assert((expr));                                                              \
        }                                                                                \
    } while (0)
#endif

#ifdef NDEBUG
#define IASSERT(expr, ...) ((void)0)
#else
#define IASSERT(expr, ...)                                                               \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            __VA_OPT__(ILOG(::ctls::e_log_level::e_error, __VA_ARGS__);)                 \
            assert((false && #expr));                                                    \
        }                                                                                \
    } while (0)
#endif