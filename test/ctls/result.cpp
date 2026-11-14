// [2026-11-14] ctls::result<T>, ctls::result<void>, ctls::ok 테스트 (result_base v5)
// - result_base 의 소멸자·move 는 protected (잘라내기·base 참조 대입·base 포인터 삭제
// 금지)
// - result<T>, result<void> 모두 result_base 를 상속, extract_failed() (& / && 둘 다)
// - operator bool 은 result_base 에 있음 (_failed == nullptr 이면 true)
// - 복사 금지, 이동·이동 대입 가능
#include <chrono>          // std::chrono::system_clock
#include <cstdint>         // std::uint_least32_t
#include <memory>          // std::unique_ptr, std::make_unique
#include <source_location> // std::source_location
#include <string>          // std::string, std::to_string
#include <string_view>     // std::string_view
#include <type_traits>     // std::is_*_v
#include <utility>         // std::move, std::declval
#include <vector>          // std::vector

#include <ctls/error/result.hpp>
#include <gtest/gtest.h>

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

    // 이름 있는 변수를 그대로 반환 (이동 생성자 필요)
    ctls::result<int> parse_named(int v) {
        auto r = parse_positive(v);
        return r;
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

    // result<void> 이름 있는 변수 반환
    ctls::result<void> save_named(bool good) {
        ctls::result<void> r;
        if (!good) {
            r = ctls::failed(std::string{"named"});
        }
        return r;
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

    ctls::result<int> pick(bool good) {
        return good ? ctls::result<int>{1}
                    : ctls::result<int>{ctls::failed(std::string{"bad"})};
    }

    // result_base 참조로 받는 공용 함수
    bool is_ok(const ctls::result_base& r) {
        return static_cast<bool>(r);
    }
} // namespace

// result_tu2.cpp 에 정의 (inline constexpr ok 가 여러 cpp 에서 같이 쓰일 때 링크 확인)
ctls::result<void> result_tu2_ok();
ctls::result<void> result_tu2_failed();

// ==================== 컴파일 단계 확인 ====================

// 복사 금지, 이동 가능
static_assert(!std::is_copy_constructible_v<ctls::result<int>>);
static_assert(!std::is_copy_assignable_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_assignable_v<ctls::result<int>>);
static_assert(!std::is_copy_constructible_v<ctls::result<void>>);
static_assert(!std::is_copy_assignable_v<ctls::result<void>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<void>>);
static_assert(std::is_nothrow_move_assignable_v<ctls::result<void>>);
// 이동 전용 값도 담고 옮길 수 있음
static_assert(std::is_move_constructible_v<ctls::result<std::unique_ptr<int>>>);
static_assert(!std::is_copy_constructible_v<ctls::result<std::unique_ptr<int>>>);

// 값 / failed_result / ok 로 다시 대입 가능 (임시 result 를 만든 뒤 이동 대입)
static_assert(std::is_assignable_v<ctls::result<int>&, int>);
static_assert(std::is_assignable_v<ctls::result<int>&, ctls::failed_result>);
static_assert(std::is_assignable_v<ctls::result<void>&, ctls::ok_result>);
static_assert(std::is_assignable_v<ctls::result<void>&, ctls::failed_result>);

// result_base 상속 구조
static_assert(std::is_base_of_v<ctls::result_base, ctls::result<int>>);
static_assert(std::is_base_of_v<ctls::result_base, ctls::result<void>>);
static_assert(std::is_convertible_v<ctls::result<int>*, ctls::result_base*>);
static_assert(!std::has_virtual_destructor_v<ctls::result_base>);
static_assert(!std::is_polymorphic_v<ctls::result_base>);
// result_base 는 직접 만들 수 없음 (생성자 protected)
static_assert(!std::is_default_constructible_v<ctls::result_base>);
static_assert(!std::is_constructible_v<ctls::result_base, ctls::failed_result>);
static_assert(!std::is_copy_constructible_v<ctls::result_base>);
// result_base 의 move·소멸자는 protected: base 로 잘라 옮기기, base 참조로 대입, base
// 포인터로 삭제 불가
static_assert(!std::is_move_constructible_v<ctls::result_base>);
static_assert(!std::is_constructible_v<ctls::result_base, ctls::result<int>&&>);
static_assert(!std::is_constructible_v<ctls::result_base, ctls::result<void>&&>);
static_assert(!std::is_move_assignable_v<ctls::result_base>);
static_assert(!std::is_assignable_v<ctls::result_base&, ctls::result<int>&&>);
static_assert(!std::is_destructible_v<ctls::result_base>);
// 가상 함수가 없어서 vtable 포인터가 붙지 않음
static_assert(sizeof(ctls::result<void>) == sizeof(std::unique_ptr<ctls::failed_result>));

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
static_assert(std::is_nothrow_constructible_v<bool, const ctls::result_base&>);

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

// extract_failed 는 result<T>, result<void> 모두 lvalue / rvalue 가능, const 는 불가
static_assert(lvalue_extract_failed<ctls::result<int>>);
static_assert(rvalue_extract_failed<ctls::result<int>>);
static_assert(!const_extract_failed<ctls::result<int>>);
static_assert(lvalue_extract_failed<ctls::result<void>>);
static_assert(rvalue_extract_failed<ctls::result<void>>);
static_assert(!const_extract_failed<ctls::result<void>>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<int>&>().extract_failed()),
              ctls::failed_result>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<int>&&>().extract_failed()),
              ctls::failed_result>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<void>&>().extract_failed()),
              ctls::failed_result>);
static_assert(std::is_same_v<
              decltype(std::declval<ctls::result<void>&&>().extract_failed()),
              ctls::failed_result>);

// 예전 이름 get_failed 는 둘 다 없음
static_assert(!has_get_failed<ctls::result<int>>);
static_assert(!has_get_failed<ctls::result<void>>);

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

TEST(ResultTest, FalseValueIsStillTrue) {
    // 담긴 값이 false 여도 result 자체는 성공
    ctls::result<bool> r = false;
    EXPECT_TRUE(r);
    EXPECT_FALSE(*r);
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

// ==================== result<T> : 이동 / 대입 ====================

TEST(ResultMoveTest, MoveConstructValue) {
    ctls::result<std::string> a = std::string(100, 'm');
    ctls::result<std::string> b = std::move(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, std::string(100, 'm'));
}

TEST(ResultMoveTest, MoveConstructFailed) {
    ctls::result<int> a =
        ctls::fails(ctls::failed(std::string{"m1"}), ctls::failed(std::string{"m2"}));
    ctls::result<int> b = std::move(a);
    ASSERT_FALSE(b);
    EXPECT_FALSE(b.has_value());
    const ctls::failed_result f = b.extract_failed();
    EXPECT_EQ(f.message, "m1");
    ASSERT_NE(f.before, nullptr);
    EXPECT_EQ(f.before->message, "m2");
}

TEST(ResultMoveTest, MoveConstructMoveOnlyValue) {
    ctls::result<std::unique_ptr<int>> a = std::make_unique<int>(3);
    ctls::result<std::unique_ptr<int>> b = std::move(a);
    ASSERT_TRUE(b);
    ASSERT_NE(*b, nullptr);
    EXPECT_EQ(**b, 3);
}

TEST(ResultMoveTest, AssignValueOverFailed) {
    ctls::result<int> r = ctls::failed(std::string{"old"});
    ASSERT_FALSE(r);
    r = 8;
    ASSERT_TRUE(r);
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(*r, 8);
}

TEST(ResultMoveTest, AssignFailedOverValue) {
    ctls::result<int> r = 1;
    ASSERT_TRUE(r);
    r = ctls::failed(std::string{"new"});
    ASSERT_FALSE(r);
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(r.extract_failed().message, "new");
}

TEST(ResultMoveTest, AssignFailedOverFailed) {
    ctls::result<int> r = ctls::failed(std::string{"first"});
    r = ctls::failed(std::string{"second"});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "second");
}

TEST(ResultMoveTest, MoveAssignFromOther) {
    ctls::result<std::string> a = std::string{"a"};
    ctls::result<std::string> b = ctls::failed(std::string{"b"});
    a = std::move(b);
    ASSERT_FALSE(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(a.extract_failed().message, "b");
}

TEST(ResultMoveTest, ReturnNamedVariable) {
    auto good = parse_named(4);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 4);

    auto bad = parse_named(-4);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "not positive: -4");
}

TEST(ResultMoveTest, StoreInVector) {
    std::vector<ctls::result<int>> v;
    for (int i = -2; i <= 2; ++i) {
        v.push_back(parse_positive(i)); // 재할당 때 이동 필요
    }
    ASSERT_EQ(v.size(), 5U);
    int ok_count = 0;
    for (auto& r : v) {
        if (r) {
            ++ok_count;
        } else {
            EXPECT_EQ(r.extract_failed().message.rfind("not positive: ", 0), 0U);
        }
    }
    EXPECT_EQ(ok_count, 2);
    EXPECT_EQ(*v[4], 2);
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

TEST(ResultVoidTest, MoveConstruct) {
    ctls::result<void> a = ctls::failed(std::string{"mv"});
    ctls::result<void> b = std::move(a);
    ASSERT_FALSE(b);
    EXPECT_EQ(b.extract_failed().message, "mv");

    ctls::result<void> c = ctls::ok;
    ctls::result<void> d = std::move(c);
    EXPECT_TRUE(d);
}

TEST(ResultVoidTest, AssignOkAndFailed) {
    ctls::result<void> r;
    r = ctls::failed(std::string{"set"});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.extract_failed().message, "set");

    r = ctls::ok;
    EXPECT_TRUE(r);

    r = ctls::failed(std::string{"again"});
    r = {}; // 기본 생성한 result<void> 로 이동 대입 = 성공
    EXPECT_TRUE(r);
}

TEST(ResultVoidTest, ReturnNamedVariable) {
    EXPECT_TRUE(save_named(true));
    auto bad = save_named(false);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "named");
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

TEST(ResultBaseTest, BoolMatchesHasValue) {
    // result<T> 의 bool 은 result_base(_failed == nullptr) 기준. 정상 상태에선 has_value
    // 와 같아야 함
    for (int i = -1; i <= 1; ++i) {
        auto r = parse_positive(i);
        EXPECT_EQ(static_cast<bool>(r), r.has_value()) << "i=" << i;
    }
    ctls::result<int> r = 1;
    r = ctls::failed(std::string{"x"});
    EXPECT_EQ(static_cast<bool>(r), r.has_value());
    r = 2;
    EXPECT_EQ(static_cast<bool>(r), r.has_value());
}
