#include <gtest/gtest.h>

#include <ctls/error/status.hpp>

#include <string>       // std::string
#include <type_traits>  // std::is_*_v
#include <utility>      // std::move
#include <vector>       // std::vector

// [2026-11-08] ctls::status 테스트

namespace {
    ctls::status ok_by_braces() { return {}; }
    ctls::status ok_by_factory() { return ctls::status::ok(); }
    ctls::status fail_by_failed(int code) { return ctls::failed("code={}", code); }

    ctls::status check(int v) {
        if (v < 0) return ctls::failed("negative: {}", v);
        return {};
    }

    ctls::status return_local(bool ok) {
        ctls::status s = ok ? ctls::status::ok() : ctls::status{ctls::failed("local")};
        return s; // 이동으로 리턴
    }
} // namespace

// 컴파일 타임 규칙
static_assert(!std::is_copy_constructible_v<ctls::status>);
static_assert(!std::is_copy_assignable_v<ctls::status>);
static_assert(std::is_nothrow_move_constructible_v<ctls::status>);
static_assert(std::is_nothrow_move_assignable_v<ctls::status>);
static_assert(std::is_nothrow_default_constructible_v<ctls::status>);
static_assert(noexcept(ctls::status::ok()));
static_assert(!std::is_convertible_v<ctls::status, bool>);           // explicit operator bool
static_assert(std::is_constructible_v<bool, ctls::status>);          // if (s) 는 가능
static_assert(std::is_convertible_v<ctls::failed_result, ctls::status>);
static_assert(!std::is_convertible_v<std::string, ctls::status>);    // explicit status(std::string)
static_assert(std::is_constructible_v<ctls::status, std::string>);

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

TEST(StatusTest, ReturnFailed) {
    const auto s = fail_by_failed(42);
    EXPECT_FALSE(s.is_ok());
    EXPECT_FALSE(static_cast<bool>(s));
    EXPECT_EQ(s.get_message(), "code=42");
}

TEST(StatusTest, FromFailedResult) {
    const ctls::status s = ctls::failed("plain");
    EXPECT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "plain");
}

TEST(StatusTest, FromFailedRuntime) {
    const std::string fmt = "rt {}";
    const ctls::status s = ctls::failed_runtime(fmt, 1);
    EXPECT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "rt 1");
}

TEST(StatusTest, ExplicitStringConstructor) {
    const ctls::status s{std::string{"explicit"}};
    EXPECT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "explicit");
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

TEST(StatusTest, ReturnLocalVariable) {
    const auto ok = return_local(true);
    EXPECT_TRUE(ok.is_ok());

    const auto ng = return_local(false);
    EXPECT_FALSE(ng.is_ok());
    EXPECT_EQ(ng.get_message(), "local");
}

TEST(StatusTest, MoveConstruct) {
    ctls::status a = ctls::failed("moved");
    const ctls::status b{std::move(a)};
    EXPECT_FALSE(b.is_ok());
    EXPECT_EQ(b.get_message(), "moved");
}

TEST(StatusTest, MoveAssign) {
    ctls::status a = ctls::failed("assign");
    ctls::status b;
    ASSERT_TRUE(b.is_ok());
    b = std::move(a);
    EXPECT_FALSE(b.is_ok());
    EXPECT_EQ(b.get_message(), "assign");
}

TEST(StatusTest, StoreInVector) {
    std::vector<ctls::status> v;
    v.push_back(ctls::status::ok());
    v.push_back(ctls::failed("e{}", 1));
    v.emplace_back(ctls::failed("e{}", 2));
    for (int i = 0; i < 100; ++i) v.emplace_back(); // 재할당 유도 (nothrow move)

    ASSERT_EQ(v.size(), 103u);
    EXPECT_TRUE(v[0].is_ok());
    EXPECT_EQ(v[1].get_message(), "e1");
    EXPECT_EQ(v[2].get_message(), "e2");
    EXPECT_TRUE(v[102].is_ok());
}

// 컴파일 에러/경고가 나야 정상인 케이스 (참고용)
//   check(1);                          // warning: ignoring return value ... 'nodiscard'
//   ctls::status a; auto b = a;        // 복사 금지
//   bool b = check(1);                 // explicit operator bool
//   ctls::status s = std::string{"x"}; // explicit status(std::string)
