#include <gtest/gtest.h>

#include <ctls/error/result.hpp>

#include <cstdint>          // std::uint_least32_t
#include <memory>           // std::unique_ptr, std::make_unique
#include <string>           // std::string
#include <string_view>      // std::string_view
#include <type_traits>      // std::is_*_v
#include <utility>          // std::move

// [2026-11-12] ctls::result<T> 테스트 (값 / 실패 / 접근 연산자 / 전파)

namespace {
    ctls::result<int> parse_positive(int v) {
        if (v <= 0) {
            return ctls::failed("not positive: {}", v);
        }
        return v;
    }

    ctls::result<std::string> to_text(int v) {
        auto r = parse_positive(v);
        if (!r) {
            return std::move(r).to_failed(); // 실패를 다른 타입 result 로 전파
        }
        return std::to_string(*r);
    }
} // namespace

// ---------------------------------------------------------------- 컴파일 타임 규칙

static_assert(std::is_base_of_v<ctls::status, ctls::result<int>>);
static_assert(!std::is_copy_constructible_v<ctls::result<int>>);
static_assert(std::is_move_constructible_v<ctls::result<int>>);
static_assert(std::is_convertible_v<int, ctls::result<int>>);
static_assert(std::is_convertible_v<ctls::failed_result, ctls::result<int>>);
static_assert(std::is_convertible_v<const char*, ctls::result<std::string>>);
static_assert(!std::is_constructible_v<ctls::result<int>, std::string>);
static_assert(!std::is_constructible_v<ctls::result<int>, ctls::status>);
static_assert(std::is_same_v<decltype(*std::declval<ctls::result<int>&>()), int&>);
static_assert(std::is_same_v<decltype(*std::declval<const ctls::result<int>&>()), const int&>);
static_assert(std::is_same_v<decltype(*std::declval<ctls::result<int>&&>()), int&&>);

// ---------------------------------------------------------------- 값

TEST(ResultTest, ValueIsOk) {
    const ctls::result<int> r = 42;
    EXPECT_TRUE(r.is_ok());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(*r, 42);
    EXPECT_TRUE(r.get_message().empty());
}

TEST(ResultTest, ImplicitConversionFromLiteral) {
    const ctls::result<std::string> r = "abc"; // const char* → std::string
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, "abc");
}

TEST(ResultTest, LvalueValueIsCopied) {
    const std::string s = "copy me";
    const ctls::result<std::string> r = s;
    EXPECT_EQ(*r, "copy me");
    EXPECT_EQ(s, "copy me"); // 원본 유지
}

TEST(ResultTest, ArrowOperator) {
    ctls::result<std::string> r = std::string{"hello"};
    EXPECT_EQ(r->size(), 5u);
    r->append("!");
    EXPECT_EQ(*r, "hello!");
}

TEST(ResultTest, ConstArrowOperator) {
    const ctls::result<std::string> r = std::string{"hello"};
    EXPECT_EQ(r->size(), 5u);
}

TEST(ResultTest, DerefIsMutable) {
    ctls::result<int> r = 1;
    *r = 7;
    EXPECT_EQ(*r, 7);
}

TEST(ResultTest, RvalueDerefMovesValue) {
    ctls::result<std::string> r = std::string(64, 'x'); // SSO 를 넘겨서 실제 이동
    const std::string out = *std::move(r);
    EXPECT_EQ(out, std::string(64, 'x'));
}

TEST(ResultTest, MoveOnlyValue) {
    ctls::result<std::unique_ptr<int>> r = std::make_unique<int>(5);
    ASSERT_TRUE(r);
    std::unique_ptr<int> p = *std::move(r);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 5);
}

TEST(ResultTest, MoveConstructKeepsValue) {
    ctls::result<std::string> a = std::string{"moved"};
    const ctls::result<std::string> b = std::move(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, "moved");
}

// ---------------------------------------------------------------- 실패

TEST(ResultTest, FailedIsNotOk) {
    const ctls::result<int> r = ctls::failed("db open failed");
    EXPECT_FALSE(r.is_ok());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.get_message(), "db open failed");
}

TEST(ResultTest, FailedWithFormat) {
    const ctls::result<std::string> r = ctls::failed("code={}", 404);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.get_message(), "code=404");
}

TEST(ResultTest, FailedKeepsLocation) {
    const std::uint_least32_t line = __LINE__ + 1;
    const ctls::result<int> r = ctls::failed("where");
    EXPECT_EQ(r.get_location().line(), line);
}

TEST(ResultTest, FailedKeepsTimestamp) {
    const auto f = ctls::failed("time");
    const auto ts = f.timestamp;
    const ctls::result<int> r = f;
    EXPECT_EQ(r.get_timestamp(), ts);
}

TEST(ResultTest, FailedHasNoChain) {
    const ctls::result<int> r = ctls::failed("x");
    EXPECT_EQ(r.get_chain(), nullptr);
}

// ---------------------------------------------------------------- 함수 리턴 / 전파

TEST(ResultTest, ReturnValueFromFunction) {
    const auto r = parse_positive(3);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 3);
}

TEST(ResultTest, ReturnFailedFromFunction) {
    const auto r = parse_positive(-1);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.get_message(), "not positive: -1");
}

TEST(ResultTest, PropagateToOtherTypeOk) {
    const auto r = to_text(12);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, "12");
}

TEST(ResultTest, PropagateToOtherTypeKeepsFailure) {
    const auto inner = parse_positive(0);
    const auto r = to_text(0);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.get_message(), "not positive: 0");
    // 위치는 처음 실패한 parse_positive 안의 줄이 그대로 전달된다
    EXPECT_EQ(r.get_location().line(), inner.get_location().line());
}

TEST(ResultTest, ConstToFailedCopiesMessage) {
    const ctls::result<int> r = ctls::failed("keep");
    const ctls::result<double> d = r.to_failed();
    EXPECT_EQ(d.get_message(), "keep");
    EXPECT_EQ(r.get_message(), "keep"); // const 버전은 원본 유지
}

TEST(ResultTest, SliceToStatus) {
    ctls::result<int> r = ctls::failed("as status");
    const ctls::status s = std::move(r); // result → status (값은 버려지고 실패 정보만 남음)
    EXPECT_FALSE(s);
    EXPECT_EQ(s.get_message(), "as status");
}
