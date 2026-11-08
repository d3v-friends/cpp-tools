#include <gtest/gtest.h>

#include <ctls/error/failed_result.hpp>

#include <chrono>           // std::chrono::system_clock
#include <cstdint>          // std::uint_least32_t
#include <format>           // std::format_error
#include <source_location>  // std::source_location
#include <string>           // std::string
#include <string_view>      // std::string_view
#include <type_traits>      // std::is_aggregate_v, std::is_same_v
#include <utility>          // std::move

// [2026-11-08] ctls::failed / ctls::failed_runtime 테스트 (message, location, timestamp)

namespace {
    using clock_type = std::chrono::system_clock;

    // function_name() 확인용. TEST 매크로 함수 이름은 gtest 버전마다 달라서 헬퍼로 고정
    ctls::failed_result failed_plain_in_helper() { return ctls::failed(std::string{"helper"}); }
    ctls::failed_result failed_fmt_in_helper(int code) { return ctls::failed("helper {}", code); }
    ctls::failed_result failed_rt_in_helper(const std::string& fmt) {
        return ctls::failed_runtime(fmt, 1);
    }

    bool ends_with_this_file(const std::source_location& loc) {
        return std::string_view{loc.file_name()}.ends_with("failed_result_test.cpp");
    }

    bool contains(const char* haystack, std::string_view needle) {
        return std::string_view{haystack}.find(needle) != std::string_view::npos;
    }

    template <class F> bool in_call_range(F&& make) {
        const auto before = clock_type::now();
        const ctls::failed_result f = make();
        const auto after = clock_type::now();
        return before <= f.timestamp && f.timestamp <= after;
    }
} // namespace

// 컴파일 타임 규칙
static_assert(std::is_aggregate_v<ctls::failed_result>);
static_assert(std::is_same_v<decltype(ctls::failed_result::location), std::source_location>);
static_assert(std::is_same_v<decltype(ctls::failed_result::timestamp), clock_type::time_point>);

// ---------------------------------------------------------------- message: failed(std::string)

TEST(FailedResultTest, PlainMessage) {
    const auto f = ctls::failed(std::string{"db open failed"});
    EXPECT_EQ(f.message, "db open failed");
}

TEST(FailedResultTest, LvalueStringIsCopied) {
    const std::string msg = "lvalue message";
    const auto f = ctls::failed(msg);
    EXPECT_EQ(f.message, "lvalue message");
    EXPECT_EQ(msg, "lvalue message"); // 원본 유지
}

TEST(FailedResultTest, RvalueStringIsMoved) {
    std::string msg(64, 'x'); // SSO 를 넘겨서 실제 이동이 일어나게
    const auto f = ctls::failed(std::move(msg));
    EXPECT_EQ(f.message, std::string(64, 'x'));
}

TEST(FailedResultTest, EmptyMessage) {
    const auto f = ctls::failed(std::string{});
    EXPECT_TRUE(f.message.empty());
}

TEST(FailedResultTest, LiteralWithoutArgsKeepsBraces) {
    // 인자가 없으면 비템플릿 failed(std::string) 이 선택되어 포맷하지 않는다
    const auto f = ctls::failed("progress {}%");
    EXPECT_EQ(f.message, "progress {}%");
}

// ---------------------------------------------------------------- message: failed(fmt, args...)

TEST(FailedResultTest, FormatRvalueArg) {
    const auto f = ctls::failed("code={}", 42);
    EXPECT_EQ(f.message, "code=42");
}

TEST(FailedResultTest, FormatLvalueAndConstArgs) {
    int n = 3;
    const int c = 7;
    const std::string path = "/tmp/a.db";
    const auto f = ctls::failed("{} {} {}", n, c, path);
    EXPECT_EQ(f.message, "3 7 /tmp/a.db");
}

TEST(FailedResultTest, FormatStringLiteralArg) {
    const auto f = ctls::failed("{}", "abc");
    EXPECT_EQ(f.message, "abc");
}

TEST(FailedResultTest, FormatSpec) {
    const auto f = ctls::failed("[{:>5}] [{:04x}]", "ab", 255);
    EXPECT_EQ(f.message, "[   ab] [00ff]");
}

TEST(FailedResultTest, FormatEscapedBraces) {
    const auto f = ctls::failed("{{{}}}", 1);
    EXPECT_EQ(f.message, "{1}");
}

TEST(FailedResultTest, ForwardRuntimeMessage) {
    const std::string other = "inner error {}";
    const auto f = ctls::failed("{}", other); // 런타임 문자열은 "{}" 로 넘긴다
    EXPECT_EQ(f.message, "inner error {}");
}

// ---------------------------------------------------------------- message: failed_runtime

TEST(FailedResultTest, RuntimeFormatString) {
    const std::string fmt = "id={} name={}";
    const auto f = ctls::failed_runtime(fmt, 10, "ciao");
    EXPECT_EQ(f.message, "id=10 name=ciao");
}

TEST(FailedResultTest, RuntimeFormatStringView) {
    constexpr std::string_view fmt = "x={}";
    int x = 5;
    const auto f = ctls::failed_runtime(fmt, x);
    EXPECT_EQ(f.message, "x=5");
}

TEST(FailedResultTest, RuntimeLiteralAndRvalueArg) {
    const auto f = ctls::failed_runtime("v={}", 9); // const ARGS& 라서 rvalue 도 OK
    EXPECT_EQ(f.message, "v=9");
}

TEST(FailedResultTest, RuntimeNoArgs) {
    const auto f = ctls::failed_runtime("no args");
    EXPECT_EQ(f.message, "no args");
}

TEST(FailedResultTest, RuntimeTooFewArgsThrows) {
    const std::string fmt = "{} {}";
    EXPECT_THROW((void)ctls::failed_runtime(fmt, 1), std::format_error);
}

TEST(FailedResultTest, RuntimeInvalidFormatThrows) {
    const std::string fmt = "{";
    EXPECT_THROW((void)ctls::failed_runtime(fmt), std::format_error);
}

// ---------------------------------------------------------------- location
// line = __LINE__ + 1; 다음 줄에서 failed 호출

TEST(FailedResultTest, LocationOfPlainString) {
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed(std::string{"plain"});
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(ends_with_this_file(f.location));
}

TEST(FailedResultTest, LocationOfLiteralWithoutArgs) {
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed("literal {}");
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(ends_with_this_file(f.location));
}

TEST(FailedResultTest, LocationOfFormat) {
    const int c = 1;
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed("x={} y={}", c, 2);
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(ends_with_this_file(f.location));
}

TEST(FailedResultTest, LocationOfRuntime) {
    const std::string fmt = "rt {}";
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed_runtime(fmt, 3);
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(ends_with_this_file(f.location));
}

TEST(FailedResultTest, LocationOfRuntimeNoArgs) {
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed_runtime("rt");
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(ends_with_this_file(f.location));
}

TEST(FailedResultTest, LocationIsCallerFunctionNotHeader) {
    // failed_result.hpp / error.hpp 가 아니라 호출한 함수가 기록되어야 함
    const auto a = failed_plain_in_helper();
    const auto b = failed_fmt_in_helper(7);
    const auto c = failed_rt_in_helper("rt {}");

    EXPECT_EQ(b.message, "helper 7");
    EXPECT_TRUE(ends_with_this_file(a.location));
    EXPECT_TRUE(ends_with_this_file(b.location));
    EXPECT_TRUE(ends_with_this_file(c.location));
    EXPECT_TRUE(contains(a.location.function_name(), "failed_plain_in_helper"));
    EXPECT_TRUE(contains(b.location.function_name(), "failed_fmt_in_helper"));
    EXPECT_TRUE(contains(c.location.function_name(), "failed_rt_in_helper"));
}

TEST(FailedResultTest, LocationDiffersPerCallSite) {
    const auto a = ctls::failed("a");
    const auto b = ctls::failed("b");
    EXPECT_EQ(b.location.line(), a.location.line() + 1);
}

// ---------------------------------------------------------------- timestamp

TEST(FailedResultTest, TimestampOfPlainString) {
    EXPECT_TRUE(in_call_range([] { return ctls::failed(std::string{"t"}); }));
}

TEST(FailedResultTest, TimestampOfFormat) {
    EXPECT_TRUE(in_call_range([] { return ctls::failed("t {}", 1); }));
}

TEST(FailedResultTest, TimestampOfRuntime) {
    const std::string fmt = "t {}";
    EXPECT_TRUE(in_call_range([&] { return ctls::failed_runtime(fmt, 1); }));
}

TEST(FailedResultTest, TimestampIsNotEpoch) {
    const auto f = ctls::failed("t");
    EXPECT_NE(f.timestamp, clock_type::time_point{});
}

TEST(FailedResultTest, TimestampIsOrderedForSequentialCalls) {
    // system_clock 은 시계 보정 시 거꾸로 갈 수 있지만, 연속 호출에서는 사실상 단조 증가
    const auto a = ctls::failed("a");
    const auto b = ctls::failed("b");
    EXPECT_TRUE(a.timestamp <= b.timestamp);
}

// 컴파일 에러가 나야 정상인 케이스 (gtest 로 자동 검증 불가, 참고용)
//   ctls::failed("{} {}", 1);          // 인자 개수 불일치 → consteval 에러
//   std::string fmt = "{}";
//   ctls::failed(fmt, 1);              // 런타임 포맷 문자열 → failed_runtime 사용
