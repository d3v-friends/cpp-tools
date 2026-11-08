#include <gtest/gtest.h>

#include <ctls/error/status.hpp>

#include <chrono>           // std::chrono::system_clock
#include <cstdint>          // std::uint_least32_t
#include <source_location>  // std::source_location
#include <string>           // std::string
#include <string_view>      // std::string_view
#include <type_traits>      // std::is_*_v
#include <utility>          // std::move
#include <vector>           // std::vector

// [2026-11-08] ctls::status 테스트 (ok/fail, message, location, timestamp, move)

namespace {
    using clock_type = std::chrono::system_clock;

    ctls::status ok_by_braces() { return {}; }
    ctls::status ok_by_factory() { return ctls::status::ok(); }
    ctls::status fail_by_format(int code) { return ctls::failed("code={}", code); }
    ctls::status fail_by_string(const std::string& msg) { return ctls::failed(msg); }
    ctls::status fail_by_runtime(const std::string& fmt, int v) { return ctls::failed_runtime(fmt, v); }

    ctls::status check(int v) {
        if (v < 0) return ctls::failed("negative: {}", v);
        return {};
    }

    ctls::status return_local(bool ok) {
        ctls::status s = ok ? ctls::status::ok() : ctls::status{ctls::failed("local")};
        return s; // 로컬 변수 리턴 → 이동
    }

    // 전파: inner 에서 실패한 위치/시각이 outer 를 거쳐도 유지되어야 함
    constexpr std::uint_least32_t k_inner_line = __LINE__ + 1;
    ctls::status inner() { return ctls::failed("inner {}", 1); }

    ctls::status outer() {
        if (auto s = inner(); !s) return s;
        return {};
    }

    bool ends_with_this_file(const std::source_location& loc) {
        return std::string_view{loc.file_name()}.ends_with("status_test.cpp");
    }

    bool contains(const char* haystack, std::string_view needle) {
        return std::string_view{haystack}.find(needle) != std::string_view::npos;
    }

    // protected 멤버를 파생 클래스(result 등)에서 쓸 수 있는지 확인용
    struct derived_status : ctls::status {
        using ctls::status::status;
        [[nodiscard]] bool raw_is_ok() const noexcept { return _is_ok; }
        [[nodiscard]] const std::string& raw_message() const noexcept { return _message; }
    };
} // namespace

// 컴파일 타임 규칙
static_assert(!std::is_copy_constructible_v<ctls::status>);
static_assert(!std::is_copy_assignable_v<ctls::status>);
static_assert(std::is_nothrow_move_constructible_v<ctls::status>);
static_assert(std::is_nothrow_move_assignable_v<ctls::status>);
static_assert(std::is_nothrow_default_constructible_v<ctls::status>);
static_assert(noexcept(ctls::status::ok()));
static_assert(!std::is_convertible_v<ctls::status, bool>);  // explicit operator bool
static_assert(std::is_constructible_v<bool, ctls::status>); // if (s) 는 가능
static_assert(std::is_convertible_v<ctls::failed_result, ctls::status>); // return ctls::failed(...)
static_assert(!std::is_constructible_v<ctls::status, std::string>);      // 실패는 failed() 로만
static_assert(!std::is_constructible_v<ctls::status, const char*>);
static_assert(std::is_same_v<decltype(std::declval<const ctls::status&>().get_message()),
                             const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<const ctls::status&>().get_location()),
                             const std::source_location&>);
static_assert(std::is_same_v<decltype(std::declval<const ctls::status&>().get_timestamp()),
                             clock_type::time_point>);

// ---------------------------------------------------------------- 성공

TEST(StatusTest, DefaultIsOk) {
    const ctls::status s;
    EXPECT_TRUE(s.is_ok());
    EXPECT_TRUE(static_cast<bool>(s));
    EXPECT_TRUE(s.get_message().empty());
}

TEST(StatusTest, ReturnBracesIsOk) {
    const auto s = ok_by_braces();
    EXPECT_TRUE(s.is_ok());
    EXPECT_TRUE(s.get_message().empty());
}

TEST(StatusTest, OkFactory) {
    const auto s = ok_by_factory();
    EXPECT_TRUE(s.is_ok());
    EXPECT_TRUE(s.get_message().empty());
}

TEST(StatusTest, OkHasEmptyLocationAndEpochTimestamp) {
    const auto s = ctls::status::ok();
    EXPECT_EQ(s.get_location().line(), 0u);
    EXPECT_EQ(std::string_view{s.get_location().file_name()}, "");
    EXPECT_EQ(s.get_timestamp(), clock_type::time_point{});
}

// ---------------------------------------------------------------- 실패

TEST(StatusTest, ReturnFailedFormat) {
    const auto s = fail_by_format(42);
    EXPECT_FALSE(s.is_ok());
    EXPECT_FALSE(static_cast<bool>(s));
    EXPECT_EQ(s.get_message(), "code=42");
}

TEST(StatusTest, ReturnFailedString) {
    const auto s = fail_by_string("plain");
    EXPECT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "plain");
}

TEST(StatusTest, ReturnFailedRuntime) {
    const auto s = fail_by_runtime("rt {}", 1);
    EXPECT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "rt 1");
}

TEST(StatusTest, FailedWithEmptyMessageIsStillFailure) {
    const ctls::status s = ctls::failed(std::string{});
    EXPECT_FALSE(s.is_ok());
    EXPECT_TRUE(s.get_message().empty());
}

TEST(StatusTest, IfInitPattern) {
    if (auto s = check(-1); !s) {
        EXPECT_EQ(s.get_message(), "negative: -1");
    } else {
        FAIL() << "check(-1) 은 실패해야 함";
    }

    if (auto s = check(1); !s) {
        FAIL() << "check(1) 은 성공해야 함: " << s.get_message();
    }
}

TEST(StatusTest, ProtectedMembersVisibleToDerived) {
    const derived_status ok;
    EXPECT_TRUE(ok.raw_is_ok());

    const derived_status ng = ctls::failed("derived {}", 1);
    EXPECT_FALSE(ng.raw_is_ok());
    EXPECT_EQ(ng.raw_message(), "derived 1");
}

// ---------------------------------------------------------------- location

TEST(StatusTest, LocationFromFailedFormat) {
    const std::uint_least32_t line = __LINE__ + 1;
    const ctls::status s = ctls::failed("loc {}", 1);
    EXPECT_EQ(s.get_location().line(), line);
    EXPECT_TRUE(ends_with_this_file(s.get_location()));
}

TEST(StatusTest, LocationFromFailedString) {
    const std::uint_least32_t line = __LINE__ + 1;
    const ctls::status s = ctls::failed(std::string{"loc"});
    EXPECT_EQ(s.get_location().line(), line);
    EXPECT_TRUE(ends_with_this_file(s.get_location()));
}

TEST(StatusTest, LocationFromFailedRuntime) {
    const std::string fmt = "loc {}";
    const std::uint_least32_t line = __LINE__ + 1;
    const ctls::status s = ctls::failed_runtime(fmt, 1);
    EXPECT_EQ(s.get_location().line(), line);
    EXPECT_TRUE(ends_with_this_file(s.get_location()));
}

TEST(StatusTest, LocationIsFunctionThatFailed) {
    const auto a = check(-1);
    const auto b = fail_by_string("x");
    const auto c = fail_by_runtime("{}", 1);
    EXPECT_TRUE(contains(a.get_location().function_name(), "check"));
    EXPECT_TRUE(contains(b.get_location().function_name(), "fail_by_string"));
    EXPECT_TRUE(contains(c.get_location().function_name(), "fail_by_runtime"));
}

TEST(StatusTest, LocationKeptWhenPropagated) {
    const auto s = outer();
    ASSERT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "inner 1");
    EXPECT_EQ(s.get_location().line(), k_inner_line); // outer 가 아니라 inner 위치
    EXPECT_TRUE(contains(s.get_location().function_name(), "inner"));
}

// ---------------------------------------------------------------- timestamp

TEST(StatusTest, TimestampSameAsFailedResult) {
    auto f = ctls::failed("ts");
    const auto ts = f.timestamp;
    const ctls::status s = std::move(f);
    EXPECT_EQ(s.get_timestamp(), ts); // status 생성 시점이 아니라 failed() 시점
}

TEST(StatusTest, TimestampInCallRange) {
    const auto before = clock_type::now();
    const auto s = check(-1);
    const auto after = clock_type::now();
    EXPECT_TRUE(before <= s.get_timestamp());
    EXPECT_TRUE(s.get_timestamp() <= after);
}

TEST(StatusTest, TimestampKeptWhenPropagated) {
    const auto before = clock_type::now();
    const auto s = outer();
    const auto after = clock_type::now();
    EXPECT_NE(s.get_timestamp(), clock_type::time_point{});
    EXPECT_TRUE(before <= s.get_timestamp());
    EXPECT_TRUE(s.get_timestamp() <= after);
}

// ---------------------------------------------------------------- 이동 / 컨테이너

TEST(StatusTest, ReturnLocalVariable) {
    const auto ok = return_local(true);
    EXPECT_TRUE(ok.is_ok());

    const auto ng = return_local(false);
    EXPECT_FALSE(ng.is_ok());
    EXPECT_EQ(ng.get_message(), "local");
}

TEST(StatusTest, MoveConstructKeepsAll) {
    ctls::status a = ctls::failed("moved");
    const auto line = a.get_location().line();
    const auto ts = a.get_timestamp();

    const ctls::status b{std::move(a)};
    EXPECT_FALSE(b.is_ok());
    EXPECT_EQ(b.get_message(), "moved");
    EXPECT_EQ(b.get_location().line(), line);
    EXPECT_EQ(b.get_timestamp(), ts);
}

TEST(StatusTest, MoveAssignOverOk) {
    ctls::status a = ctls::failed("assign");
    const auto line = a.get_location().line();
    const auto ts = a.get_timestamp();

    ctls::status b;
    ASSERT_TRUE(b.is_ok());
    b = std::move(a);
    EXPECT_FALSE(b.is_ok());
    EXPECT_EQ(b.get_message(), "assign");
    EXPECT_EQ(b.get_location().line(), line);
    EXPECT_EQ(b.get_timestamp(), ts);
}

TEST(StatusTest, MoveAssignOkOverFailed) {
    ctls::status b = ctls::failed("old");
    ASSERT_FALSE(b.is_ok());
    b = ctls::status::ok();
    EXPECT_TRUE(b.is_ok());
    EXPECT_TRUE(b.get_message().empty());
    EXPECT_EQ(b.get_location().line(), 0u);
    EXPECT_EQ(b.get_timestamp(), clock_type::time_point{});
}

TEST(StatusTest, StoreInVector) {
    std::vector<ctls::status> v;
    v.push_back(ctls::status::ok());
    v.push_back(ctls::failed("e{}", 1));
    v.emplace_back(ctls::failed("e{}", 2));
    const auto ts = v[1].get_timestamp();
    for (int i = 0; i < 100; ++i) v.emplace_back(); // 재할당 유도 (nothrow move)

    ASSERT_EQ(v.size(), 103u);
    EXPECT_TRUE(v[0].is_ok());
    EXPECT_EQ(v[1].get_message(), "e1");
    EXPECT_EQ(v[2].get_message(), "e2");
    EXPECT_NE(v[1].get_location().line(), 0u); // 재할당 후에도 위치 유지
    EXPECT_EQ(v[1].get_timestamp(), ts);       // 재할당 후에도 시각 유지
    EXPECT_TRUE(v[102].is_ok());
}

// 컴파일 에러/경고가 나야 정상인 케이스 (참고용)
//   check(1);                          // warning: ignoring return value ... 'nodiscard'
//   ctls::status a; auto b = a;        // 복사 금지
//   bool b = check(1);                 // explicit operator bool
//   ctls::status s{std::string{"x"}};  // string 생성자 없음 → ctls::failed() 사용
