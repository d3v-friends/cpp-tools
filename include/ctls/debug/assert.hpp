#pragma once
#include <cassert>
#include <source_location>

#include "logger.hpp"

#ifdef NDEBUG
#define I_ASSERT(expr, str) ((void)0)
#define I_ASSERT_F(expr, fmt, ...) ((void)0)
#else
#define I_ASSERT_F(expr, fmt, ...)                                                       \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            ctls::logger::printf_line(                                                   \
                std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__);        \
            assert((expr));                                                              \
        }                                                                                \
    } while (0)

#define I_ASSERT(expr, str)                                                              \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            ctls::logger::print_line(std::source_location::current(), str);              \
            assert((expr));                                                              \
        }                                                                                \
    } while (0)
#endif