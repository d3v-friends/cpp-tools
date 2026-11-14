// [2026-11-14] ctls::result<T>, ctls::result<void>, ctls::ok 테스트 (result_base 버전)
// - result<T>, result<void> 모두 result_base 상속
// - result: 복사 금지, 이동 가능
// - result_base: 생성/이동/소멸이 protected -> 외부에서 단독 사용 불가, 파생 잘라내기(slicing) 불가
// - 소멸자 virtual 아님 -> result_base* 로 delete 할 수 없음 (컴파일 에러로 막힘)
// - result<> == result<void> == void_result (기본 템플릿 인자)
// - extract_failed 는 한 번만 호출 가능 (디버그 빌드에서 두 번째 호출은 IASSERT 로 abort)
//   && 버전은 & 버전을 불러서 같은 검사를 쓴다. 릴리스(NDEBUG)에서는 검사 없음
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

    // extract_failed 를 lvalue / rvalue / const lvalue 에서 부를 수 있는지
    template <class R>
    concept lvalue_extract_failed = requires(R& r) { r.extract_failed(); };
    template <class R>
    concept rvalue_extract_failed = requires(R&& r) { std::move(r).extract_failed(); };
    template <class R>
    concept const_extract_failed = requires(const R& r) { r.extract_failed(); };

    // 예전 이름 get_failed 가 남아 있는지
    template <class R>
    concept has_get_failed = requires(R& r) { std::move(r).get_failed(); };

    // has_value 가 있는지 (result<void> 에는 없어야 함)
    template <class R>
    concept has_has_value = requires(const R& r) { r.has_value(); };

    // ---- 실제 사용 형태 헬퍼 ----
    ctls::result<int> parse_positive(int v) {
        if (v <= 0) {
            return ctls::failed("not positive: {}", v);
        }
        return v;
    }

    // lvalue 로 꺼내서 다른 T 로 실패 전파
    ctls::result<std::string> to_label(int v) {
        auto r = parse_positive(v);
        if (!r) {
            return r.extract_failed();
        }
        return "n=" + std::to_string(*r);
    }

    // rvalue 로 꺼내서 다른 T 로 실패 전파
    ctls::result<std::string> to_label_moved(int v) {
        auto r = parse_positive(v);
        if (!r) {
            return std::move(r).extract_failed();
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

    // result<int> → result<void>
    ctls::result<void> check(int v) {
        auto r = parse_positive(v);
        if (!r) {
            return r.extract_failed();
        }
        return ctls::ok;
    }

    // result<void> → result<int> (lvalue 로 꺼냄)
    ctls::result<int> check_then_value(int v) {
        auto r = check(v);
        if (!r) {
            return r.extract_failed();
        }
        return v * 10;
    }

    // result<void> → result<int> (rvalue 로 꺼냄)
    ctls::result<int> check_then_value_moved(int v) {
        auto r = check(v);
        if (!r) {
            return std::move(r).extract_failed();
        }
        return v * 10;
    }

    // 삼항 연산자로 바로 만든 prvalue 반환
    ctls::result<int> pick(bool good) {
        return good ? ctls::result<int>{1} : ctls::result<int>{ctls::failed(std::string{"bad"})};
    }

    // result_base 참조로 받는 공용 함수 (upcast 는 참조라 가능)
    bool is_ok(const ctls::result_base& r) {
        return static_cast<bool>(r);
    }
} // namespace

// result_tu2.cpp 에 정의 (inline constexpr ok 가 여러 cpp 에서 같이 쓰일 때 링크 확인)
ctls::result<void> result_tu2_ok();
ctls::result<void> result_tu2_failed();

// ==================== 컴파일 단계 확인 ====================

// result: 복사 금지, 이동 가능
static_assert(!std::is_copy_constructible_v<ctls::result<int>>);
static_assert(!std::is_copy_assignable_v<ctls::result<int>>);
static_assert(std::is_move_constructible_v<ctls::result<int>>);
static_assert(std::is_move_assignable_v<ctls::result<int>>);
static_assert(!std::is_copy_constructible_v<ctls::result<void>>);
static_assert(!std::is_copy_assignable_v<ctls::result<void>>);
static_assert(std::is_move_constructible_v<ctls::result<void>>);
static_assert(std::is_move_assignable_v<ctls::result<void>>);

// result_base 는 외부에서 단독 사용 불가 (생성/이동/소멸 모두 protected)
static_assert(std::is_base_of_v<ctls::result_base, ctls::result<int>>);
static_assert(std::is_base_of_v<ctls::result_base, ctls::result<void>>);
static_assert(std::is_convertible_v<ctls::result<int>*, ctls::result_base*>);
static_assert(!std::is_default_constructible_v<ctls::result_base>);
static_assert(!std::is_copy_constructible_v<ctls::result_base>);
static_assert(!std::is_move_constructible_v<ctls::result_base>);
static_assert(!std::is_destructible_v<ctls::result_base>);
// 파생 객체를 result_base 로 잘라내 복사/이동 불가 (위에서 확인한 생성자들이 전부 막힘)

// 소멸자는 virtual 이 아니지만 result_base* 로 delete 할 수 없어 slicing delete 원천 차단
static_assert(!std::has_virtual_destructor_v<ctls::result_base>);
static_assert(std::is_destructible_v<ctls::result<int>>);
static_assert(std::is_destructible_v<ctls::result<void>>);

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
static_assert(!std::is_copy_constructible_v<ctls::failed_result>);
static_assert(std::is_constructible_v<ctls::result<int>, ctls::failed_result&&>);
static_assert(std::is_constructible_v<ctls::result<void>, ctls::failed_result&&>);

// 대입: 값/failed_result/ok 는 임시 result 를 만들어 move 대입으로 동작
static_assert(std::is_assignable_v<ctls::result<int>&, int>);
static_assert(std::is_assignable_v<ctls::result<int>&, ctls::failed_result>);
static_assert(std::is_assignable_v<ctls::result<void>&, ctls::ok_result>);
static_assert(std::is_assignable_v<ctls::result<void>&, ctls::failed_result>);
// result 자체 복사가 delete 라 const result& 에서는 대입 불가
static_assert(!std::is_assignable_v<ctls::result<int>&, const ctls::result<int>&>);

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
static_assert(!std::is_convertible_v<const ctls::result<int>&, bool>);
static_assert(std::is_constructible_v<bool, const ctls::result<int>&>);
static_assert(!std::is_convertible_v<const ctls::result<void>&, bool>);
static_assert(std::is_constructible_v<bool, const ctls::result<void>&>);

// has_value 는 result<T> 만
static_assert(has_has_value<ctls::result<int>>);
static_assert(!has_has_value<ctls::result<void>>);

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

// extract_failed 는 result<T>, result<void>, result_base 참조 모두에서
// lvalue / rvalue 로 호출 가능, const 는 불가
static_assert(lvalue_extract_failed<ctls::result<int>>);
static_assert(rvalue_extract_failed<ctls::result<int>>);
static_assert(!const_extract_failed<ctls::result<int>>);
static_assert(lvalue_extract_failed<ctls::result<void>>);
static_assert(rvalue_extract_failed<ctls::result<void>>);
static_assert(!const_extract_failed<ctls::result<void>>);
static_assert(lvalue_extract_failed<ctls::result_base>);
static_assert(rvalue_extract_failed<ctls::result_base>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<int>&>().extract_failed()),
              ctls::failed_result>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<void>&&>().extract_failed()),
              ctls::failed_result>);

// 예전 이름 get_failed 는 둘 다 없음
static_assert(!has_get_failed<ctls::result<int>>);
static_assert(!has_get_failed<ctls::result<void>>);

// result<> / void_result 는 result<void> 와 같은 타입
static_assert(std::is_same_v<ctls::result<>, ctls::result<void>>);
static_assert(std::is_same_v<ctls::result_void, ctls::result<void>>);

// 꺼냄 표시(_is_extracted) 가 move 를 막지 않음
static_assert(std::is_nothrow_move_constructible_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_assignable_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<void>>);
static_assert(std::is_nothrow_move_assignable_v<ctls::result<void>>);

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

TEST(ResultTest, MoveConstructKeepsValue) {
    ctls::result<std::string> a = std::string(80, 'x'); // SSO 를 넘기는 길이
    ctls::result<std::string> b = std::move(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, std::string(80, 'x'));
}

TEST(ResultTest, MoveAssign) {
    ctls::result<std::string> a = std::string{"src"};
    ctls::result<std::string> b = std::string{"dst"};
    b = std::move(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, "src");
}

TEST(ResultTest, AssignValueReplacesContent) {
    // r = 값 → 임시 result 로 만들어 move 대입
    ctls::result<int> r = 1;
    r = 42;
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 42);
}

TEST(ResultTest, AssignFailedThenRecover) {
    ctls::result<int> r = 1;
    r = ctls::failed(std::string{"late"}); // 실패로 교체
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "late");

    r = 7; // 다시 성공으로 복구 (_failed 포인터가 비워짐)
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 7);
}

// ==================== result<T> : 실패 / extract_failed ====================

TEST(ResultTest, FailedIsFalse) {
    ctls::result<int> r = ctls::failed(std::string{"boom"});
    EXPECT_FALSE(r);
    EXPECT_FALSE(r.has_value());
}

TEST(ResultTest, ExtractFailedLvalue) {
    ctls::result<int> r = ctls::failed("code {} / {}", 7, "x");
    ASSERT_FALSE(r);
    const ctls::failed_result f = r.extract_failed();
    EXPECT_EQ(f.message, "code 7 / x");
}

TEST(ResultTest, ExtractFailedRvalue) {
    ctls::result<int> r = ctls::failed("code {} / {}", 7, "x");
    ASSERT_FALSE(r);
    const ctls::failed_result f = std::move(r).extract_failed();
    EXPECT_EQ(f.message, "code 7 / x");
}

TEST(ResultTest, ExtractFailedFromTemporary) {
    EXPECT_EQ(parse_positive(-3).extract_failed().message, "not positive: -3");
}

TEST(ResultTest, ExtractFailedRuntimeMessage) {
    const std::string fmt = "rt {}";
    ctls::result<int> r = ctls::failed_runtime(fmt, 3);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "rt 3");
}

TEST(ResultTest, ExtractFailedKeepsLocation) {
    const auto line = __LINE__ + 1;
    ctls::result<int> r = ctls::failed(std::string{"loc"});
    ASSERT_FALSE(r);
    const ctls::failed_result f = r.extract_failed();
    EXPECT_EQ(f.location.line(), static_cast<std::uint_least32_t>(line));
    EXPECT_TRUE(is_this_file(f.location));
}

TEST(ResultTest, ExtractFailedKeepsTimestamp) {
    const auto before = clock_type::now();
    ctls::result<int> r = ctls::failed(std::string{"t"});
    const auto after = clock_type::now();
    const ctls::failed_result f = r.extract_failed();
    EXPECT_LE(before, f.timestamp);
    EXPECT_LE(f.timestamp, after);
}

TEST(ResultTest, ExtractFailedKeepsChain) {
    ctls::result<int> r =
        ctls::fails(ctls::failed(std::string{"e1"}), ctls::failed(std::string{"e2"}));
    ASSERT_FALSE(r);
    const ctls::failed_result f = r.extract_failed();
    EXPECT_EQ(f.message, "e1");
    ASSERT_NE(f.before, nullptr);
    EXPECT_EQ(f.before->message, "e2");
    EXPECT_EQ(f.before->before, nullptr);
}

TEST(ResultTest, ExtractFailedLeavesResultFalse) {
    // _failed 포인터는 남겨두므로 꺼낸 뒤에도 성공으로 바뀌지 않음
    ctls::result<int> r = ctls::failed(std::string{"once"});
    const ctls::failed_result f = r.extract_failed();
    EXPECT_EQ(f.message, "once");
    EXPECT_FALSE(r);
    EXPECT_FALSE(r.has_value());
}

TEST(ResultTest, FailedFromMovedVariable) {
    ctls::failed_result f = ctls::failed(std::string{"moved"});
    ctls::result<int> r = std::move(f);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "moved");
}

TEST(ResultTest, MoveFailedResultTransfersError) {
    ctls::result<int> a = ctls::failed(std::string{"boom"});
    ctls::result<int> b = std::move(a);
    ASSERT_FALSE(b);
    EXPECT_EQ(b.extract_failed().message, "boom");
    // 현재 구현 동작: _failed 포인터가 옮겨가 원본은 bool=true 가 된다.
    // 값도 없어서 *b 하면 UB — 이동된 원본은 절대 쓰면 안 된다는 기록으로 둠
    EXPECT_TRUE(a);
    EXPECT_FALSE(a.has_value());
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
    EXPECT_EQ(r.extract_failed().message, "not positive: -2");
}

TEST(ResultTest, ReturnTernaryPrvalue) {
    auto good = pick(true);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 1);

    auto bad = pick(false);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "bad");
}

TEST(ResultTest, PropagateToOtherTypeLvalue) {
    auto ok_r = to_label(2);
    ASSERT_TRUE(ok_r);
    EXPECT_EQ(*ok_r, "n=2");

    auto bad = to_label(0);
    ASSERT_FALSE(bad);
    const ctls::failed_result f = bad.extract_failed();
    EXPECT_EQ(f.message, "not positive: 0");
    // 위치는 전파한 곳이 아니라 처음 실패한 곳(parse_positive) 그대로
    EXPECT_TRUE(is_this_file(f.location));
    EXPECT_NE(
        std::string_view{f.location.function_name()}.find("parse_positive"),
        std::string_view::npos);
}

TEST(ResultTest, PropagateToOtherTypeRvalue) {
    auto ok_r = to_label_moved(5);
    ASSERT_TRUE(ok_r);
    EXPECT_EQ(*ok_r, "n=5");

    auto bad = to_label_moved(-1);
    ASSERT_FALSE(bad);
    EXPECT_EQ(std::move(bad).extract_failed().message, "not positive: -1");
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
    EXPECT_EQ(r.extract_failed().message, "save failed");
}

TEST(ResultVoidTest, ExtractFailedRvalue) {
    auto r = save_failed();
    ASSERT_FALSE(r);
    EXPECT_EQ(std::move(r).extract_failed().message, "save failed");
}

TEST(ResultVoidTest, ExtractFailedFromTemporary) {
    EXPECT_EQ(save_failed().extract_failed().message, "save failed");
}

TEST(ResultVoidTest, ExtractFailedLeavesResultFalse) {
    // result<void> 는 _failed == nullptr 이 성공이라, 꺼낸 뒤 포인터를 비우면 성공으로
    // 뒤집힘. 포인터를 남겨두는지 확인
    ctls::result<void> r = ctls::failed(std::string{"once"});
    const ctls::failed_result f = r.extract_failed();
    EXPECT_EQ(f.message, "once");
    EXPECT_FALSE(r);
}

TEST(ResultVoidTest, FailedKeepsLocation) {
    const auto line = __LINE__ + 1;
    ctls::result<void> r = ctls::failed(std::string{"v"});
    ASSERT_FALSE(r);
    const ctls::failed_result f = r.extract_failed();
    EXPECT_EQ(f.location.line(), static_cast<std::uint_least32_t>(line));
    EXPECT_TRUE(is_this_file(f.location));
}

TEST(ResultVoidTest, FailedKeepsChain) {
    ctls::result<void> r =
        ctls::fails(ctls::failed(std::string{"a"}), ctls::failed(std::string{"b"}));
    ASSERT_FALSE(r);
    const ctls::failed_result f = r.extract_failed();
    EXPECT_EQ(f.message, "a");
    ASSERT_NE(f.before, nullptr);
    EXPECT_EQ(f.before->message, "b");
}

TEST(ResultVoidTest, AssignOkAndFailed) {
    ctls::result<void> r;
    r = ctls::failed(std::string{"assigned"});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "assigned");

    r = ctls::ok; // 성공으로 복구
    EXPECT_TRUE(r);
}

TEST(ResultVoidTest, MoveKeepsState) {
    ctls::result<void> a = ctls::ok;
    ctls::result<void> b = std::move(a);
    EXPECT_TRUE(b);

    ctls::result<void> fa = ctls::failed(std::string{"mv"});
    ctls::result<void> fb = std::move(fa);
    ASSERT_FALSE(fb);
    EXPECT_EQ(fb.extract_failed().message, "mv");
}

TEST(ResultVoidTest, PropagateFromResultInt) {
    EXPECT_TRUE(check(1));

    auto r = check(-5);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "not positive: -5");
}

TEST(ResultVoidTest, PropagateToResultIntLvalue) {
    auto good = check_then_value(2);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 20);

    auto bad = check_then_value(0);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "not positive: 0");
}

TEST(ResultVoidTest, PropagateToResultIntRvalue) {
    auto good = check_then_value_moved(3);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 30);

    auto bad = check_then_value_moved(-4);
    ASSERT_FALSE(bad);
    EXPECT_EQ(std::move(bad).extract_failed().message, "not positive: -4");
}

TEST(ResultVoidTest, OkAcrossTranslationUnits) {
    EXPECT_TRUE(result_tu2_ok());
    auto r = result_tu2_failed();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "tu2");
}

// ==================== result_base ====================

TEST(ResultBaseTest, BoolThroughBaseRef) {
    ctls::result<int> v = 1;
    ctls::result<int> fv = ctls::failed(std::string{"x"});
    ctls::result<void> o = ctls::ok;
    ctls::result<void> fo = ctls::failed(std::string{"y"});
    EXPECT_TRUE(is_ok(v));
    EXPECT_FALSE(is_ok(fv));
    EXPECT_TRUE(is_ok(o));
    EXPECT_FALSE(is_ok(fo));
}

TEST(ResultBaseTest, ExtractFailedThroughBaseRef) {
    ctls::result<int> r = ctls::failed(std::string{"base"});
    ctls::result_base& b = r;
    EXPECT_EQ(b.extract_failed().message, "base");
}

// ==================== result<> / void_result ====================

TEST(ResultDefaultVoidTest, EmptyAngleBrackets) {
    ctls::result<> a;
    EXPECT_TRUE(a);
    ctls::result<> b = ctls::failed(std::string{"angle"});
    ASSERT_FALSE(b);
    EXPECT_EQ(b.extract_failed().message, "angle");
}

TEST(ResultDefaultVoidTest, VoidResultAlias) {
    ctls::result_void a = ctls::ok;
    EXPECT_TRUE(a);
    ctls::result_void b = save_failed(); // result<void> 를 그대로 받음
    ASSERT_FALSE(b);
    EXPECT_EQ(b.extract_failed().message, "save failed");
}

// ==================== extract_failed 한 번만 ====================

TEST(ExtractOnceTest, ReassignFailedAllowsExtractAgain) {
    // 새 failed 를 대입하면 새 result 가 들어오므로 꺼냄 표시도 초기화
    ctls::result<int> r = ctls::failed(std::string{"first"});
    EXPECT_EQ(r.extract_failed().message, "first");
    r = ctls::failed(std::string{"second"});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "second");

    ctls::result<void> v = ctls::failed(std::string{"v1"});
    EXPECT_EQ(v.extract_failed().message, "v1");
    v = ctls::failed(std::string{"v2"});
    EXPECT_EQ(std::move(v).extract_failed().message, "v2");
}

TEST(ExtractOnceTest, MovedResultCanExtractOnce) {
    // 꺼내기 전에 옮긴 result 는 옮겨간 쪽에서 한 번 꺼낼 수 있음
    ctls::result<int> a = ctls::failed(std::string{"mv"});
    ctls::result<int> b = std::move(a);
    EXPECT_EQ(b.extract_failed().message, "mv");
}

TEST(ExtractOnceTest, RvalueExtractUsesSameCheck) {
    // && 버전은 & 버전을 불러서 같은 표시를 켠다. 한 번은 정상, 이후 상태도 실패 유지
    ctls::result<int> r = ctls::failed(std::string{"rv"});
    EXPECT_EQ(std::move(r).extract_failed().message, "rv");
    EXPECT_FALSE(r); // NOLINT(bugprone-use-after-move)

    ctls::result<void> v = ctls::failed(std::string{"vrv"});
    EXPECT_EQ(std::move(v).extract_failed().message, "vrv");
    EXPECT_FALSE(v); // NOLINT(bugprone-use-after-move)
}

TEST(ExtractOnceTest, MoveAssignFreshFailedOverExtracted) {
    // 이미 꺼낸 result 에 다른 (아직 안 꺼낸) result 를 move 대입하면 표시도 같이 바뀜
    ctls::result<int> r = ctls::failed(std::string{"old"});
    (void)r.extract_failed();
    ctls::result<int> fresh = ctls::failed(std::string{"new"});
    r = std::move(fresh);
    EXPECT_EQ(r.extract_failed().message, "new");
}

TEST(ExtractOnceTest, AssignValueThenFailedAfterExtract) {
    // 꺼냄 → 값으로 성공 → 다시 실패: 마지막 실패는 꺼낼 수 있음
    ctls::result<int> r = ctls::failed(std::string{"a"});
    (void)r.extract_failed();
    r = 5;
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 5);
    r = ctls::failed(std::string{"b"});
    EXPECT_EQ(r.extract_failed().message, "b");
}

#ifdef NDEBUG
TEST(ExtractOnceTest, ReleaseDoesNotCheckTwice) {
    // 릴리스: IASSERT 가 ((void)0) 이라 std::exchange 도 실행 안 됨 → 두 번째도 abort 없음.
    // 두 번째 값은 이미 move 된 failed_result 라 내용은 확인하지 않음
    ctls::result<int> r = ctls::failed(std::string{"rel"});
    EXPECT_EQ(r.extract_failed().message, "rel");
    (void)r.extract_failed();
    EXPECT_FALSE(r);
}
#endif

#ifndef NDEBUG
// 디버그 빌드: 두 번째 extract_failed 는 IASSERT → assert 로 abort.
// &, && 어느 조합으로 불러도 두 번째는 막혀야 함.
// 메시지는 logger 로 나가서 stderr 정규식은 비워둠 (abort 여부만 확인)
TEST(ExtractOnceDeathTest, LvalueTwice) {
    ctls::result<int> r = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto a = r.extract_failed();
            auto b = r.extract_failed();
        },
        "");
}

TEST(ExtractOnceDeathTest, RvalueTwice) {
    ctls::result<int> r = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto a = std::move(r).extract_failed();
            auto b = std::move(r).extract_failed(); // NOLINT(bugprone-use-after-move)
        },
        "");
}

TEST(ExtractOnceDeathTest, LvalueThenRvalue) {
    ctls::result<int> r = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto a = r.extract_failed();
            auto b = std::move(r).extract_failed();
        },
        "");
}

TEST(ExtractOnceDeathTest, RvalueThenLvalue) {
    ctls::result<int> r = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto a = std::move(r).extract_failed();
            auto b = r.extract_failed(); // NOLINT(bugprone-use-after-move)
        },
        "");
}

TEST(ExtractOnceDeathTest, VoidLvalueTwice) {
    ctls::result<void> r = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto a = r.extract_failed();
            auto b = r.extract_failed();
        },
        "");
}

TEST(ExtractOnceDeathTest, ThroughBaseRefTwice) {
    ctls::result<int> r = ctls::failed(std::string{"x"});
    ctls::result_base& base = r;
    EXPECT_DEATH(
        {
            auto a = r.extract_failed();
            auto b = base.extract_failed();
        },
        "");
}

TEST(ExtractOnceDeathTest, VoidRvalueTwice) {
    ctls::result<void> r = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto a = std::move(r).extract_failed();
            auto b = std::move(r).extract_failed(); // NOLINT(bugprone-use-after-move)
        },
        "");
}

TEST(ExtractOnceDeathTest, VoidLvalueThenRvalue) {
    ctls::result<void> r = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto a = r.extract_failed();
            auto b = std::move(r).extract_failed();
        },
        "");
}

TEST(ExtractOnceDeathTest, MoveAfterExtractKeepsMark) {
    // 꺼낸 뒤 옮기면 꺼냄 표시(bool)도 같이 옮겨가서 옮겨간 쪽에서도 못 꺼냄
    ctls::result<int> a = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto f = a.extract_failed();
            ctls::result<int> b = std::move(a);
            auto g = b.extract_failed();
        },
        "");
}

TEST(ExtractOnceDeathTest, ExtractFromSuccess) {
    // 실패가 아닌데 꺼내면 "not failed result" 로 abort
    ctls::result<int> r = 1;
    ctls::result<void> v;
    EXPECT_DEATH({ auto a = r.extract_failed(); }, "");
    EXPECT_DEATH({ auto a = v.extract_failed(); }, "");
}
#endif

TEST(ResultBaseTest, SlicingToBaseDoesNotCompile) {
    // 결과물이 아니라 참조로만 다룰 수 있음 (위 static_assert 들이 컴파일 단계에서 보장).
    // 아래 주석을 풀면 "calling a protected constructor/destructor" 로 컴파일 실패한다.
    // ctls::result<int> src = 1;
    // ctls::result_base sliced = std::move(src);
    // std::unique_ptr<ctls::result_base> p =
    //     std::make_unique<ctls::result<int>>(7);  // p 가 delete → protected 소멸자
    SUCCEED();
}
