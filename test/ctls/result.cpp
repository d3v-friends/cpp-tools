// [2026-11-13] ctls::result<T>, ctls::result<void>, ctls::ok 테스트
#include <gtest/gtest.h>

#include <ctls/error/result.hpp>

#include <chrono>          // std::chrono::system_clock
#include <cstdint>         // std::uint_least32_t
#include <memory>          // std::unique_ptr, std::make_unique
#include <source_location> // std::source_location
#include <string>          // std::string, std::to_string
#include <string_view>     // std::string_view
#include <type_traits>     // std::is_*_v
#include <utility>         // std::move, std::declval

namespace {
    using clock_type = std::chrono::system_clock;

    // 파일 이름을 직접 적지 않고 컴파일러가 알려주는 현재 파일 경로와 비교 (파일 이름이
    // 바뀌어도 안 깨짐)
    bool is_this_file(const std::source_location& loc) {
        return std::string_view{loc.file_name()} ==
               std::source_location::current().file_name();
    }

    // T 를 받는 함수에 {} 를 넘길 수 있는지 (= return {}; 가 되는지)
    template <class T>
    concept empty_brace_ok = requires(void (*fn)(T)) { fn({}); };

    // get_failed 를 lvalue 에서 부를 수 있는지 (rvalue 전용이어야 함)
    template <class R>
    concept lvalue_get_failed = requires(R& r) { r.get_failed(); };

    // ---- 실제 사용 형태 헬퍼 ----
    ctls::result<int> parse_positive(int v) {
        if (v <= 0) {
            return ctls::failed("not positive: {}", v);
        }
        return v;
    }

    ctls::result<std::string> to_label(int v) {
        auto r = parse_positive(v);
        if (!r) {
            return std::move(r).get_failed(); // 다른 T 로 실패 전파
        }
        return "n=" + std::to_string(*r);
    }

    ctls::result<void> save_ok_tag() {
        return ctls::ok;
    }
    ctls::result<void> save_empty_brace() {
        return {};
    }
    ctls::result<void> save_failed() {
        return ctls::failed(std::string{"save failed"});
    }

    ctls::result<void> check(int v) {
        auto r = parse_positive(v);
        if (!r) {
            return std::move(r).get_failed(); // result<int> → result<void>
        }
        return ctls::ok;
    }

    ctls::result<int> check_then_value(int v) {
        auto r = check(v);
        if (!r) {
            return std::move(r).get_failed(); // result<void> → result<int>
        }
        return v * 10;
    }
} // namespace

// result_tu2.cpp 에 정의 (inline constexpr ok 가 여러 cpp 에서 같이 쓰일 때 링크 확인)
ctls::result<void> result_tu2_ok();
ctls::result<void> result_tu2_failed();

// ==================== 컴파일 단계 확인 ====================

// 복사 금지, 이동 허용
static_assert(!std::is_copy_constructible_v<ctls::result<int>>);
static_assert(!std::is_copy_assignable_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_assignable_v<ctls::result<int>>);
static_assert(!std::is_copy_constructible_v<ctls::result<void>>);
static_assert(!std::is_copy_assignable_v<ctls::result<void>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<void>>);
static_assert(std::is_nothrow_move_assignable_v<ctls::result<void>>);

// result<T> 는 기본 생성 불가 (값도 실패도 없는 상태를 막음) → return {}; 컴파일 에러
static_assert(!std::is_default_constructible_v<ctls::result<int>>);
static_assert(!empty_brace_ok<ctls::result<int>>);

// result<void> 는 기본 생성 = 성공, return {}; 가능
static_assert(std::is_default_constructible_v<ctls::result<void>>);
static_assert(empty_brace_ok<ctls::result<void>>);

// 자동 변환
static_assert(std::is_convertible_v<int, ctls::result<int>>);
static_assert(std::is_convertible_v<const char*, ctls::result<std::string>>);
static_assert(
    std::is_convertible_v<std::unique_ptr<int>, ctls::result<std::unique_ptr<int>>>);
static_assert(std::is_convertible_v<ctls::failed_result, ctls::result<int>>);
static_assert(std::is_convertible_v<ctls::failed_result, ctls::result<void>>);

// failed_result 변수는 복사가 안 되니 std::move 가 필요
// (lvalue → result 의 is_convertible 판정은 컴파일러마다 달라서 복사 불가 자체를 확인)
static_assert(!std::is_copy_constructible_v<ctls::failed_result>);
static_assert(std::is_constructible_v<ctls::result<int>, ctls::failed_result&&>);
static_assert(std::is_constructible_v<ctls::result<void>, ctls::failed_result&&>);

// 변환 안 되는 타입은 막힘
static_assert(!std::is_constructible_v<ctls::result<int>, std::string>);
static_assert(!std::is_constructible_v<ctls::result<std::string>, int*>);

// ok 는 result<void> 전용
static_assert(std::is_convertible_v<ctls::ok_result, ctls::result<void>>);
static_assert(std::is_convertible_v<const ctls::ok_result&, ctls::result<void>>);
static_assert(std::is_nothrow_constructible_v<ctls::result<void>, ctls::ok_result>);
static_assert(!std::is_constructible_v<ctls::result<int>, ctls::ok_result>);
static_assert(!std::is_constructible_v<ctls::result<std::string>, ctls::ok_result>);
static_assert(!std::is_constructible_v<ctls::result<bool>, ctls::ok_result>);

// ok_result 는 {} 로 몰래 만들어지지 않음 (explicit 기본 생성자)
static_assert(!empty_brace_ok<ctls::ok_result>);
static_assert(std::is_empty_v<ctls::ok_result>);

// bool 변환은 explicit
static_assert(!std::is_convertible_v<ctls::result<int>, bool>);
static_assert(std::is_constructible_v<bool, ctls::result<int>>);
static_assert(!std::is_convertible_v<ctls::result<void>, bool>);
static_assert(std::is_constructible_v<bool, ctls::result<void>>);

// 연산자 반환 타입
static_assert(std::is_same_v<decltype(*std::declval<ctls::result<int>&>()), int&>);
static_assert(
    std::is_same_v<decltype(*std::declval<const ctls::result<int>&>()), const int&>);
static_assert(std::is_same_v<decltype(*std::declval<ctls::result<int>&&>()), int>);
static_assert(
    std::is_same_v<decltype(std::declval<ctls::result<int>&>().operator->()), int*>);
static_assert(std::is_same_v<
              decltype(std::declval<const ctls::result<int>&>().operator->()),
              const int*>);

// get_failed 는 rvalue 에서만 (std::move(r).get_failed())
static_assert(!lvalue_get_failed<ctls::result<int>>);
static_assert(!lvalue_get_failed<ctls::result<void>>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<int>&&>().get_failed()),
              ctls::failed_result>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<void>&&>().get_failed()),
              ctls::failed_result>);

// ==================== result<T> : 값 ====================

TEST(ResultTest, ValueIsTrue) {
    ctls::result<int> r = 42;
    EXPECT_TRUE(r);
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(*r, 42);
}

TEST(ResultTest, ImplicitConversion) {
    ctls::result<std::string> s = "abc";
    ASSERT_TRUE(s);
    EXPECT_EQ(*s, "abc");

    ctls::result<long long> l = 7;
    ASSERT_TRUE(l);
    EXPECT_EQ(*l, 7LL);
}

TEST(ResultTest, ModifyThroughStar) {
    ctls::result<int> r = 1;
    *r = 5;
    EXPECT_EQ(*r, 5);
}

TEST(ResultTest, ConstStar) {
    const ctls::result<int> r = 3;
    EXPECT_TRUE(r);
    EXPECT_EQ(*r, 3);
}

TEST(ResultTest, Arrow) {
    ctls::result<std::string> r = std::string{"hello"};
    EXPECT_EQ(r->size(), 5U);
    r->append("!");
    EXPECT_EQ(*r, "hello!");

    const ctls::result<std::string> c = std::string{"xy"};
    EXPECT_EQ(c->size(), 2U);
}

TEST(ResultTest, RvalueStarMovesValue) {
    ctls::result<std::string> r = std::string(100, 'a'); // SSO 를 넘기는 길이
    std::string out = *std::move(r);
    EXPECT_EQ(out, std::string(100, 'a'));
}

TEST(ResultTest, MoveOnlyValue) {
    ctls::result<std::unique_ptr<int>> r = std::make_unique<int>(9);
    ASSERT_TRUE(r);
    ASSERT_NE(*r, nullptr);
    EXPECT_EQ(**r, 9);

    std::unique_ptr<int> p = *std::move(r);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 9);
}

TEST(ResultTest, TemporaryRvalueStar) {
    // *f() 가 값으로 돌려줘서 임시 객체가 사라져도 안전
    const int v = *parse_positive(4);
    EXPECT_EQ(v, 4);
}

// ==================== result<T> : 실패 ====================

TEST(ResultTest, FailedIsFalse) {
    ctls::result<int> r = ctls::failed(std::string{"boom"});
    EXPECT_FALSE(r);
    EXPECT_FALSE(r.has_value());
}

TEST(ResultTest, GetFailedMessage) {
    ctls::result<int> r = ctls::failed("code {} / {}", 7, "x");
    ASSERT_FALSE(r);
    const ctls::failed_result f = std::move(r).get_failed();
    EXPECT_EQ(f.message, "code 7 / x");
}

TEST(ResultTest, GetFailedRuntimeMessage) {
    const std::string fmt = "rt {}";
    ctls::result<int> r = ctls::failed_runtime(fmt, 3);
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "rt 3");
}

TEST(ResultTest, GetFailedKeepsLocation) {
    const auto line = __LINE__ + 1;
    ctls::result<int> r = ctls::failed(std::string{"loc"});
    ASSERT_FALSE(r);
    const ctls::failed_result f = std::move(r).get_failed();
    EXPECT_EQ(f.location.line(), static_cast<std::uint_least32_t>(line));
    EXPECT_TRUE(is_this_file(f.location));
}

TEST(ResultTest, GetFailedKeepsTimestamp) {
    const auto before = clock_type::now();
    ctls::result<int> r = ctls::failed(std::string{"t"});
    const auto after = clock_type::now();
    const ctls::failed_result f = std::move(r).get_failed();
    EXPECT_LE(before, f.timestamp);
    EXPECT_LE(f.timestamp, after);
}

TEST(ResultTest, GetFailedKeepsChain) {
    ctls::result<int> r =
        ctls::fails(ctls::failed(std::string{"e1"}), ctls::failed(std::string{"e2"}));
    ASSERT_FALSE(r);
    const ctls::failed_result f = std::move(r).get_failed();
    EXPECT_EQ(f.message, "e1");
    ASSERT_NE(f.before, nullptr);
    EXPECT_EQ(f.before->message, "e2");
    EXPECT_EQ(f.before->before, nullptr);
}

TEST(ResultTest, FailedFromMovedVariable) {
    ctls::failed_result f = ctls::failed(std::string{"moved"});
    ctls::result<int> r = std::move(f);
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "moved");
}

// ==================== result<T> : move / 대입 ====================

TEST(ResultTest, MoveConstructValue) {
    ctls::result<int> a = 5;
    ctls::result<int> b = std::move(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, 5);
}

TEST(ResultTest, MoveConstructFailed) {
    ctls::result<int> a = ctls::failed(std::string{"mv"});
    ctls::result<int> b = std::move(a);
    ASSERT_FALSE(b);
    EXPECT_EQ(std::move(b).get_failed().message, "mv");
}

TEST(ResultTest, MovedFromFailedStaysFalse) {
    // result<T> 는 _value 기준이라 move 뒤에도 원본이 성공으로 바뀌지 않음
    ctls::result<int> a = ctls::failed(std::string{"x"});
    ctls::result<int> b = std::move(a);
    EXPECT_FALSE(a); // NOLINT(bugprone-use-after-move)
    EXPECT_FALSE(b);
}

TEST(ResultTest, AssignFailedOverValue) {
    ctls::result<int> r = 1;
    r = ctls::failed(std::string{"later"});
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "later");
}

TEST(ResultTest, AssignValueOverFailed) {
    ctls::result<int> r = ctls::failed(std::string{"first"});
    r = 8;
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 8);
}

// ==================== result<T> : 함수에서 돌려받기 ====================

TEST(ResultTest, ReturnValue) {
    auto r = parse_positive(3);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 3);
}

TEST(ResultTest, ReturnFailed) {
    auto r = parse_positive(-2);
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "not positive: -2");
}

TEST(ResultTest, PropagateToOtherType) {
    auto ok_r = to_label(2);
    ASSERT_TRUE(ok_r);
    EXPECT_EQ(*ok_r, "n=2");

    auto bad = to_label(0);
    ASSERT_FALSE(bad);
    const ctls::failed_result f = std::move(bad).get_failed();
    EXPECT_EQ(f.message, "not positive: 0");
    // 위치는 전파한 곳이 아니라 처음 실패한 곳(parse_positive) 그대로
    EXPECT_TRUE(is_this_file(f.location));
    EXPECT_NE(
        std::string_view{f.location.function_name()}.find("parse_positive"),
        std::string_view::npos);
}

TEST(ResultTest, IfInit) {
    int seen = 0;
    if (auto r = parse_positive(6)) {
        seen = *r;
    }
    EXPECT_EQ(seen, 6);

    bool failed_branch = false;
    if (auto r = parse_positive(-1); !r) {
        failed_branch = true;
    }
    EXPECT_TRUE(failed_branch);
}

// ==================== result<void> ====================

TEST(ResultVoidTest, DefaultIsOk) {
    ctls::result<void> r;
    EXPECT_TRUE(r);
}

TEST(ResultVoidTest, OkTagIsOk) {
    ctls::result<void> r = ctls::ok;
    EXPECT_TRUE(r);
}

TEST(ResultVoidTest, ReturnOkTag) {
    EXPECT_TRUE(save_ok_tag());
}

TEST(ResultVoidTest, ReturnEmptyBrace) {
    EXPECT_TRUE(save_empty_brace());
}

TEST(ResultVoidTest, ReturnFailed) {
    auto r = save_failed();
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "save failed");
}

TEST(ResultVoidTest, FailedKeepsLocation) {
    const auto line = __LINE__ + 1;
    ctls::result<void> r = ctls::failed(std::string{"v"});
    ASSERT_FALSE(r);
    const ctls::failed_result f = std::move(r).get_failed();
    EXPECT_EQ(f.location.line(), static_cast<std::uint_least32_t>(line));
    EXPECT_TRUE(is_this_file(f.location));
}

TEST(ResultVoidTest, FailedKeepsChain) {
    ctls::result<void> r =
        ctls::fails(ctls::failed(std::string{"a"}), ctls::failed(std::string{"b"}));
    ASSERT_FALSE(r);
    const ctls::failed_result f = std::move(r).get_failed();
    EXPECT_EQ(f.message, "a");
    ASSERT_NE(f.before, nullptr);
    EXPECT_EQ(f.before->message, "b");
}

TEST(ResultVoidTest, PropagateFromResultInt) {
    EXPECT_TRUE(check(1));

    auto r = check(-5);
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "not positive: -5");
}

TEST(ResultVoidTest, PropagateToResultInt) {
    auto good = check_then_value(2);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 20);

    auto bad = check_then_value(0);
    ASSERT_FALSE(bad);
    EXPECT_EQ(std::move(bad).get_failed().message, "not positive: 0");
}

TEST(ResultVoidTest, AssignFailedOverOk) {
    ctls::result<void> r = ctls::ok;
    r = ctls::failed(std::string{"now failed"});
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "now failed");
}

TEST(ResultVoidTest, AssignOkOverFailed) {
    ctls::result<void> r = ctls::failed(std::string{"was failed"});
    r = ctls::ok;
    EXPECT_TRUE(r);

    ctls::result<void> r2 = ctls::failed(std::string{"was failed"});
    r2 = {};
    EXPECT_TRUE(r2);
}

TEST(ResultVoidTest, MoveConstructFailed) {
    ctls::result<void> a = ctls::failed(std::string{"mv"});
    ctls::result<void> b = std::move(a);
    ASSERT_FALSE(b);
    EXPECT_EQ(std::move(b).get_failed().message, "mv");
}

TEST(ResultVoidTest, OkAcrossTranslationUnits) {
    EXPECT_TRUE(result_tu2_ok());
    auto r = result_tu2_failed();
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).get_failed().message, "tu2");
}
