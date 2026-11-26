// [2026-11-14] ctls::result<T>, ctls::result<void>, ctls::ok 테스트 (result_base 버전)
// - result<T>, result<void> 모두 result_base 상속
// - result: 복사 금지, 이동 가능
// - result_base: 생성/이동/소멸이 protected -> 외부에서 단독 사용 불가, 파생 잘라내기(slicing) 불가
// - 소멸자 virtual 아님 -> result_base* 로 delete 할 수 없음 (컴파일 에러로 막힘)
// - result<> == result<void> == void_result (기본 템플릿 인자)
// - extract_failed 는 한 번만 호출 가능 (디버그 빌드에서 두 번째 호출은 IASSERT 로 abort)
//   && 버전은 & 버전을 불러서 같은 검사를 쓴다. 릴리스(NDEBUG)에서는 검사 없음
// - fails(result...) : result<T>/result<void> 여러 개에서 실패를 꺼내 입력 순서대로 체인으로 연결
// - result(result_base&&) : 실패한 result 를 다른 타입 result 로 그대로 전파 (if (!cur) return cur;)
//   성공한 result 를 넘기면 디버그 빌드에서 IASSERT("propagate success result") 로 abort
#include <gtest/gtest.h>

#include <ctls/error/result.hpp>

#include <chrono>          // std::chrono::system_clock
#include <cstdint>         // std::uint_least32_t
#include <memory>          // std::unique_ptr, std::make_unique
#include <source_location> // std::source_location
#include <string>          // std::string, std::to_string
#include <string_view>     // std::string_view
#include <type_traits>     // std::is_*_v
#include <utility>         // std::move, std::declval, std::forward
#include <vector>          // std::vector

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

    // ---- fails(result...) 헬퍼 ----

    // 주어진 인자로 ctls::fails(...) 를 부를 수 있는지
    template <class... A>
    concept can_fails = requires(A&&... a) { ctls::fails(std::forward<A>(a)...); };

    // 체인(message → before → before ...) 을 앞에서부터 메시지 목록으로
    std::vector<std::string> chain_messages(const ctls::failed_result& f) {
        std::vector<std::string> out;
        for (const ctls::failed_result* it = &f; it != nullptr; it = it->before.get()) {
            out.push_back(it->message);
        }
        return out;
    }

    ctls::result<int> load_a(bool good) {
        if (!good) {
            return ctls::failed(std::string{"a failed"});
        }
        return 1;
    }

    ctls::result<void> load_b(bool good) {
        if (!good) {
            return ctls::failed(std::string{"b failed"});
        }
        return ctls::ok;
    }

    // 실제 사용 형태: 둘 다 실패면 한 번에 묶어서 전파
    ctls::result<int> load_both(bool a_good, bool b_good) {
        auto a = load_a(a_good);
        auto b = load_b(b_good);
        if (!a && !b) {
            return ctls::fails(a, b);
        }
        if (!a) {
            return a.extract_failed();
        }
        if (!b) {
            return b.extract_failed();
        }
        return *a + 1;
    }

    // ---- result(result_base&&) 전파 헬퍼 ----
    // 실제 사용 형태: auto cur = query(...); if (!cur) { return cur; }
    // C++20 에서는 return 변수; 가 먼저 rvalue 로 취급돼서 std::move 없이 전파 생성자가 선택됨

    template <class... ARGS> ctls::result<int> query(bool good, ARGS&&... args) {
        if (!good) {
            return ctls::failed("query failed: {}", sizeof...(args));
        }
        return static_cast<int>(sizeof...(args));
    }

    // result<int> → result<std::string>
    template <class... ARGS>
    ctls::result<std::string> query_label(bool good, ARGS&&... args) {
        auto cur = query(good, std::forward<ARGS>(args)...);
        if (!cur) {
            return cur;
        }
        return "rows=" + std::to_string(*cur);
    }

    // result<int> → result<void>
    ctls::result<void> query_void(bool good) {
        auto cur = query(good, 1);
        if (!cur) {
            return cur;
        }
        return ctls::ok;
    }

    // result<void> → result<int>
    ctls::result<int> void_to_int(bool good) {
        auto cur = load_b(good);
        if (!cur) {
            return cur;
        }
        return 100;
    }

    // result<void> → result<void> (같은 타입: 기존 move 생성자)
    ctls::result<void> void_to_void(bool good) {
        auto cur = load_b(good);
        if (!cur) {
            return cur;
        }
        return {};
    }

    // result<std::string> → result<std::unique_ptr<int>> (move-only 값 타입으로)
    ctls::result<std::unique_ptr<int>> label_to_ptr(bool good) {
        auto cur = query_label(good, 1, 2);
        if (!cur) {
            return cur;
        }
        return std::make_unique<int>(static_cast<int>(cur->size()));
    }

    // 여러 단계 전파: int → void → string
    ctls::result<std::string> multi_hop(bool good) {
        auto cur = query_void(good);
        if (!cur) {
            return cur;
        }
        return std::string{"ok"};
    }

    // 체인(fails) 이 있는 실패를 전파
    ctls::result<std::string> chain_propagate() {
        ctls::result<int> cur = ctls::fails(load_a(false), load_b(false));
        if (!cur) {
            return cur;
        }
        return std::string{"never"};
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

// fails(result...) : result_base 를 상속한 타입만, 종류 섞어서 받을 수 있음
static_assert(can_fails<ctls::result<int>>);
static_assert(can_fails<ctls::result<void>>);
static_assert(can_fails<ctls::result<int>&>); // 이름 있는 변수도 받음 (안에서 꺼냄)
static_assert(can_fails<ctls::result_base&>);
static_assert(can_fails<ctls::result<int>, ctls::result<void>, ctls::result<std::string>>);
static_assert(std::is_same_v<
              decltype(ctls::fails(std::declval<ctls::result<int>>())),
              ctls::failed_result>);
// failed_result 를 받는 기존 fails 도 그대로
static_assert(can_fails<ctls::failed_result, ctls::failed_result>);
// result 가 아닌 타입, failed_result 와 result 를 섞는 것은 불가
static_assert(!can_fails<int>);
static_assert(!can_fails<std::string>);
static_assert(!can_fails<ctls::failed_result, ctls::result<int>>);
static_assert(!can_fails<ctls::result<int>, ctls::failed_result>);
// [헤더 수정 후 주석 해제] 지금 헤더에서는 둘 다 requires 를 통과해버림
// - 인자 0개: fails() 가 자기 자신을 다시 불러 무한 재귀 (실행 시 segfault)
// - const result: requires 는 통과하고 본문에서 extract_failed 를 못 찾아 컴파일 에러
// static_assert(!can_fails<>);
// static_assert(!can_fails<const ctls::result<int>&>);

// result(result_base&&) : 다른 타입 result 의 rvalue 에서 생성 (암시적 변환 → return cur; 가능)
static_assert(std::is_convertible_v<ctls::result<int>&&, ctls::result<std::string>>);
static_assert(std::is_convertible_v<ctls::result<void>&&, ctls::result<int>>);
static_assert(std::is_convertible_v<ctls::result<int>&&, ctls::result<void>>);
static_assert(std::is_convertible_v<ctls::result<std::string>&&, ctls::result<void>>);
static_assert(std::is_constructible_v<ctls::result<int>, ctls::result_base&&>);
static_assert(std::is_constructible_v<ctls::result<void>, ctls::result_base&&>);
// lvalue / const 는 받지 않음 → result<std::string> r = cur; 처럼 몰래 꺼내가는 것을 막음
static_assert(!std::is_constructible_v<ctls::result<std::string>, ctls::result<int>&>);
static_assert(!std::is_constructible_v<ctls::result<void>, ctls::result<int>&>);
static_assert(!std::is_constructible_v<ctls::result<int>, ctls::result_base&>);
static_assert(!std::is_constructible_v<ctls::result<std::string>, const ctls::result<int>&>);
static_assert(!std::is_constructible_v<ctls::result<std::string>, const ctls::result<int>&&>);
// 전파 생성자가 생겨도 같은 타입 복사는 여전히 금지, move 는 nothrow 유지
static_assert(!std::is_copy_constructible_v<ctls::result<int>>);
static_assert(!std::is_copy_constructible_v<ctls::result<void>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<int>>);
// 값 변환 생성자와 섞이지 않음: result<int> 가 bool/int 값으로 바뀌어 들어가지 않음
static_assert(!std::is_convertible_v<ctls::result<int>&&, bool>);
static_assert(!std::is_convertible_v<ctls::result<int>&&, int>);

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

// ==================== fails(result...) ====================

TEST(ResultFailsTest, SingleResult) {
    const ctls::failed_result f = ctls::fails(load_a(false));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"a failed"}));
}

TEST(ResultFailsTest, KeepsArgumentOrder) {
    const ctls::failed_result f = ctls::fails(load_a(false), load_b(false));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"a failed", "b failed"}));

    const ctls::failed_result g = ctls::fails(load_b(false), load_a(false));
    EXPECT_EQ(chain_messages(g), (std::vector<std::string>{"b failed", "a failed"}));
}

TEST(ResultFailsTest, MixedResultTypes) {
    ctls::result<int> i = ctls::failed(std::string{"int"});
    ctls::result<void> v = ctls::failed(std::string{"void"});
    ctls::result<std::string> s = ctls::failed(std::string{"string"});
    ctls::result<> d = ctls::failed(std::string{"default"});
    ctls::result_void rv = ctls::failed(std::string{"alias"});

    const ctls::failed_result f = ctls::fails(i, v, s, d, rv);
    EXPECT_EQ(
        chain_messages(f),
        (std::vector<std::string>{"int", "void", "string", "default", "alias"}));
}

TEST(ResultFailsTest, LvalueArgumentsAreExtracted) {
    // 이름 있는 변수를 넘기면 안에서 꺼낸다. 꺼낸 뒤에도 실패 상태는 그대로
    ctls::result<int> a = ctls::failed(std::string{"la"});
    ctls::result<void> b = ctls::failed(std::string{"lb"});
    const ctls::failed_result f = ctls::fails(a, b);
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"la", "lb"}));
    EXPECT_FALSE(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_FALSE(b);
}

TEST(ResultFailsTest, MovedArguments) {
    ctls::result<int> a = ctls::failed(std::string{"ma"});
    ctls::result<void> b = ctls::failed(std::string{"mb"});
    const ctls::failed_result f = ctls::fails(std::move(a), std::move(b));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"ma", "mb"}));
}

TEST(ResultFailsTest, ThroughBaseReferences) {
    ctls::result<int> a = ctls::failed(std::string{"ba"});
    ctls::result<void> b = ctls::failed(std::string{"bb"});
    ctls::result_base& ra = a;
    ctls::result_base& rb = b;
    const ctls::failed_result f = ctls::fails(ra, rb);
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"ba", "bb"}));
}

TEST(ResultFailsTest, NestedChainsAppendInOrder) {
    // 이미 체인이 있는 result 를 넘기면 각 체인이 끝에 차례로 이어짐
    ctls::result<int> a =
        ctls::fails(ctls::failed(std::string{"e1"}), ctls::failed(std::string{"e2"}));
    ctls::result<void> b = ctls::failed(std::string{"e3"});
    ctls::result<std::string> c =
        ctls::fails(ctls::failed(std::string{"e4"}), ctls::failed(std::string{"e5"}));

    const ctls::failed_result f = ctls::fails(a, b, c);
    EXPECT_EQ(
        chain_messages(f), (std::vector<std::string>{"e1", "e2", "e3", "e4", "e5"}));
}

TEST(ResultFailsTest, KeepsEachLocation) {
    const auto line_a = __LINE__ + 1;
    ctls::result<int> a = ctls::failed(std::string{"pa"});
    const auto line_b = __LINE__ + 1;
    ctls::result<void> b = ctls::failed(std::string{"pb"});

    const ctls::failed_result f = ctls::fails(a, b);
    ASSERT_NE(f.before, nullptr);
    EXPECT_TRUE(is_this_file(f.location));
    EXPECT_EQ(f.location.line(), static_cast<std::uint_least32_t>(line_a));
    EXPECT_TRUE(is_this_file(f.before->location));
    EXPECT_EQ(f.before->location.line(), static_cast<std::uint_least32_t>(line_b));
}

TEST(ResultFailsTest, ConvertsToResult) {
    ctls::result<int> r = ctls::fails(load_a(false), load_b(false));
    ASSERT_FALSE(r);
    EXPECT_EQ(
        chain_messages(r.extract_failed()),
        (std::vector<std::string>{"a failed", "b failed"}));

    ctls::result<void> v = ctls::fails(load_b(false));
    ASSERT_FALSE(v);
    EXPECT_EQ(v.extract_failed().message, "b failed");
}

TEST(ResultFailsTest, PropagateFromFunction) {
    auto both = load_both(false, false);
    ASSERT_FALSE(both);
    EXPECT_EQ(
        chain_messages(both.extract_failed()),
        (std::vector<std::string>{"a failed", "b failed"}));

    auto only_a = load_both(false, true);
    ASSERT_FALSE(only_a);
    EXPECT_EQ(chain_messages(only_a.extract_failed()), (std::vector<std::string>{"a failed"}));

    auto only_b = load_both(true, false);
    ASSERT_FALSE(only_b);
    EXPECT_EQ(chain_messages(only_b.extract_failed()), (std::vector<std::string>{"b failed"}));

    auto good = load_both(true, true);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 2);
}

// ==================== result(result_base&&) : return cur; 전파 ====================

TEST(ResultPropagateTest, IntToStringWithForwardedArgs) {
    auto good = query_label(true, 1, std::string{"a"}, 3.0);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, "rows=3");

    auto bad = query_label(false, 1, 2);
    ASSERT_FALSE(bad);
    EXPECT_FALSE(bad.has_value());
    EXPECT_EQ(bad.extract_failed().message, "query failed: 2");
}

TEST(ResultPropagateTest, IntToVoid) {
    EXPECT_TRUE(query_void(true));

    auto bad = query_void(false);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "query failed: 1");
}

TEST(ResultPropagateTest, VoidToInt) {
    auto good = void_to_int(true);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, 100);

    auto bad = void_to_int(false);
    ASSERT_FALSE(bad);
    EXPECT_FALSE(bad.has_value());
    EXPECT_EQ(bad.extract_failed().message, "b failed");
}

TEST(ResultPropagateTest, VoidToVoidSameType) {
    EXPECT_TRUE(void_to_void(true));

    auto bad = void_to_void(false);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "b failed");
}

TEST(ResultPropagateTest, ToMoveOnlyValueType) {
    auto good = label_to_ptr(true);
    ASSERT_TRUE(good);
    ASSERT_NE(*good, nullptr);
    EXPECT_EQ(**good, 6); // "rows=2"

    auto bad = label_to_ptr(false);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "query failed: 2");
}

TEST(ResultPropagateTest, MultiHop) {
    auto good = multi_hop(true);
    ASSERT_TRUE(good);
    EXPECT_EQ(*good, "ok");

    auto bad = multi_hop(false);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.extract_failed().message, "query failed: 1");
}

TEST(ResultPropagateTest, KeepsOriginalLocation) {
    // 위치는 전파한 곳이 아니라 처음 실패한 곳(query) 그대로
    auto bad = query_label(false);
    ASSERT_FALSE(bad);
    const ctls::failed_result f = bad.extract_failed();
    EXPECT_TRUE(is_this_file(f.location));
    EXPECT_NE(
        std::string_view{f.location.function_name()}.find("query"), std::string_view::npos);
    EXPECT_EQ(
        std::string_view{f.location.function_name()}.find("query_label"),
        std::string_view::npos);
}

TEST(ResultPropagateTest, KeepsChain) {
    auto bad = chain_propagate();
    ASSERT_FALSE(bad);
    EXPECT_EQ(
        chain_messages(bad.extract_failed()),
        (std::vector<std::string>{"a failed", "b failed"}));
}

TEST(ResultPropagateTest, ExplicitMoveConstruct) {
    // return 이 아닌 곳에서는 std::move 로 명시해야 함
    ctls::result<int> src = ctls::failed(std::string{"explicit"});
    ctls::result<std::string> dst = std::move(src);
    ASSERT_FALSE(dst);
    EXPECT_EQ(dst.extract_failed().message, "explicit");
}

TEST(ResultPropagateTest, ThroughBaseRvalue) {
    ctls::result<void> src = ctls::failed(std::string{"base"});
    ctls::result_base& base = src;
    ctls::result<int> dst = std::move(base);
    ASSERT_FALSE(dst);
    EXPECT_FALSE(dst.has_value());
    EXPECT_EQ(dst.extract_failed().message, "base");
}

TEST(ResultPropagateTest, SourceIsEmptiedAfterPropagate) {
    // 현재 구현 동작: _failed 포인터가 옮겨가 원본은 bool=true 가 된다.
    // 전파한 원본은 다시 쓰면 안 된다는 기록으로 둠 (MoveFailedResultTransfersError 와 같은 동작)
    ctls::result<int> src = ctls::failed(std::string{"gone"});
    ctls::result<std::string> dst = std::move(src);
    ASSERT_FALSE(dst);
    EXPECT_TRUE(src); // NOLINT(bugprone-use-after-move)
    EXPECT_FALSE(src.has_value());
}

TEST(ResultPropagateTest, AssignFromOtherTypeRvalue) {
    // 대입도 임시 result 를 만들어 move 대입으로 동작
    ctls::result<std::string> dst = std::string{"before"};
    ctls::result<int> src = ctls::failed(std::string{"assign"});
    dst = std::move(src);
    ASSERT_FALSE(dst);
    EXPECT_FALSE(dst.has_value());
    EXPECT_EQ(dst.extract_failed().message, "assign");
}

TEST(ResultPropagateTest, PropagatedThenFails) {
    // 전파받은 result 도 fails(result...) 에 그대로 넘길 수 있음
    auto a = query_label(false, 1);
    auto b = void_to_int(false);
    const ctls::failed_result f = ctls::fails(a, b);
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"query failed: 1", "b failed"}));
}

#ifdef NDEBUG
TEST(ResultPropagateTest, ReleaseDoesNotCheckSuccess) {
    // 릴리스: IASSERT 가 사라져 성공 result 를 넘겨도 막지 않음 → bool=true 인데 값 없음.
    // *r 는 UB 라 확인하지 않음. if (!cur) 확인을 빼먹으면 안 된다는 기록
    ctls::result<int> good = 1;
    ctls::result<std::string> r = std::move(good);
    EXPECT_TRUE(r);
    EXPECT_FALSE(r.has_value());
}
#endif

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

// fails(result...) 도 안에서 extract_failed 를 부르므로 같은 검사에 걸림
TEST(ResultFailsDeathTest, SuccessArgumentAborts) {
    ctls::result<int> bad = ctls::failed(std::string{"x"});
    ctls::result<void> good;
    EXPECT_DEATH({ auto f = ctls::fails(bad, good); }, "");
}

TEST(ResultFailsDeathTest, AlreadyExtractedArgumentAborts) {
    ctls::result<int> a = ctls::failed(std::string{"x"});
    ctls::result<void> b = ctls::failed(std::string{"y"});
    EXPECT_DEATH(
        {
            auto first = a.extract_failed();
            auto f = ctls::fails(a, b);
        },
        "");
}

TEST(ResultFailsDeathTest, ExtractAfterFailsAborts) {
    ctls::result<int> a = ctls::failed(std::string{"x"});
    ctls::result<void> b = ctls::failed(std::string{"y"});
    EXPECT_DEATH(
        {
            auto f = ctls::fails(a, b);
            auto again = a.extract_failed();
        },
        "");
}

TEST(ResultFailsDeathTest, SameResultTwiceAborts) {
    ctls::result<int> a = ctls::failed(std::string{"x"});
    EXPECT_DEATH({ auto f = ctls::fails(a, a); }, "");
}

// result(result_base&&) : 성공한 result 를 전파하면 abort (if (!cur) 확인을 빼먹은 경우)
TEST(ResultPropagateDeathTest, SuccessToOtherTypeAborts) {
    ctls::result<int> good = 1;
    EXPECT_DEATH({ ctls::result<std::string> r = std::move(good); }, "");
}

TEST(ResultPropagateDeathTest, SuccessVoidToIntAborts) {
    ctls::result<void> good;
    EXPECT_DEATH({ ctls::result<int> r = std::move(good); }, "");
}

TEST(ResultPropagateDeathTest, SuccessIntToVoidAborts) {
    ctls::result<int> good = 1;
    EXPECT_DEATH({ ctls::result<void> r = std::move(good); }, "");
}

TEST(ResultPropagateDeathTest, ExtractedMarkTravels) {
    // 꺼낸 뒤 전파하면 꺼냄 표시도 같이 넘어가서 받은 쪽에서 못 꺼냄
    ctls::result<int> src = ctls::failed(std::string{"x"});
    EXPECT_DEATH(
        {
            auto f = src.extract_failed();
            ctls::result<std::string> dst = std::move(src);
            auto g = dst.extract_failed();
        },
        "");
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
