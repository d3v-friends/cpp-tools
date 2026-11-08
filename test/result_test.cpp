#include <gtest/gtest.h>

#include <ctls/error/result.hpp>

#include <memory>       // std::unique_ptr, std::make_unique
#include <string>       // std::string, std::to_string
#include <string_view>  // std::string_view
#include <type_traits>  // std::is_*_v
#include <utility>      // std::move
#include <vector>       // std::vector

// [2026-11-08] ctls::result<T> 테스트

namespace {
    struct point {
        int x;
        int y;
    };

    struct no_default {
        explicit no_default(int v) : value{v} {}
        int value;
    };

    struct explicit_only {
        explicit explicit_only(int v) : value{v} {}
        int value;
    };

    ctls::result<int> parse(int v) {
        if (v < 0) return ctls::failed("negative: {}", v);
        return v * 2;
    }

    ctls::result<std::string> name_literal() { return "ciao"; }

    ctls::result<std::string> name_pointer() {
        const char* p = "pointer";
        return p;
    }

    ctls::result<std::string> name_lvalue() {
        std::string s = "lvalue";
        return s;
    }

    ctls::result<std::string> name_failed() { return ctls::failed("bad {}", 1); }

    ctls::result<point> make_point() { return {{1, 2}}; }

    ctls::result<no_default> make_no_default(bool ok) {
        if (!ok) return ctls::failed("no value");
        return no_default{9};
    }

    ctls::result<std::unique_ptr<int>> make_ptr() { return std::make_unique<int>(5); }

    ctls::result<bool> flag(int v) {
        if (v < 0) return ctls::failed("invalid");
        return v != 0; // false 도 성공 값
    }

    ctls::result<std::string> return_local(bool ok) {
        ctls::result<std::string> r = ok ? ctls::result<std::string>{"local"}
                                         : ctls::result<std::string>{ctls::failed("local fail")};
        return r; // 이동으로 리턴
    }

    // result 실패 → status 로 전파
    ctls::status check(int v) {
        auto r = parse(v);
        if (!r) return std::move(r);
        return {};
    }

    // result<int> 실패 → result<std::string> 로 전파
    ctls::result<std::string> to_str(int v) {
        auto r = parse(v);
        if (!r) return ctls::failed(r.get_message());
        return std::to_string(*r);
    }
} // namespace

// 컴파일 타임 규칙
static_assert(std::is_base_of_v<ctls::status, ctls::result<int>>);
static_assert(!std::is_default_constructible_v<ctls::result<int>>);           // 값 없는 성공 금지
static_assert(!std::is_copy_constructible_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<int>>);
static_assert(std::is_nothrow_move_constructible_v<ctls::result<std::string>>);
static_assert(!std::is_copy_constructible_v<ctls::result<std::unique_ptr<int>>>);
static_assert(std::is_move_constructible_v<ctls::result<std::unique_ptr<int>>>);
static_assert(!std::is_convertible_v<ctls::result<int>, bool>);                // explicit operator bool
static_assert(std::is_convertible_v<int, ctls::result<int>>);
static_assert(std::is_convertible_v<const char*, ctls::result<std::string>>);
static_assert(std::is_convertible_v<ctls::failed_result, ctls::result<int>>);
static_assert(std::is_convertible_v<ctls::failed_result, ctls::result<std::string>>);
static_assert(!std::is_convertible_v<std::string_view, ctls::result<std::string>>); // string_view→string 은 explicit
static_assert(!std::is_convertible_v<int, ctls::result<explicit_only>>);
static_assert(std::is_convertible_v<ctls::result<int>&&, ctls::status>);       // base 로 이동 가능

TEST(ResultTest, ReturnValue) {
    const auto r = parse(21);
    ASSERT_TRUE(r.is_ok());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(*r, 42);
    EXPECT_TRUE(r.get_message().empty());
}

TEST(ResultTest, ReturnFailed) {
    const auto r = parse(-1);
    EXPECT_FALSE(r.is_ok());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.get_message(), "negative: -1");
}

TEST(ResultTest, StringFromLiteral) {
    const auto r = name_literal();
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(*r, "ciao");
}

TEST(ResultTest, StringFromPointer) {
    const auto r = name_pointer();
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(*r, "pointer");
}

TEST(ResultTest, StringFromLvalue) {
    const auto r = name_lvalue();
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(*r, "lvalue");
}

TEST(ResultTest, StringFailedStaysFailure) {
    // result<std::string> 에서 실패 메세지가 값으로 들어가면 안 된다
    const auto r = name_failed();
    EXPECT_FALSE(r.is_ok());
    EXPECT_EQ(r.get_message(), "bad 1");
}

TEST(ResultTest, BraceInit) {
    const auto r = make_point();
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r->x, 1);
    EXPECT_EQ(r->y, 2);
}

TEST(ResultTest, NoDefaultConstructibleType) {
    const auto ok = make_no_default(true);
    ASSERT_TRUE(ok.is_ok());
    EXPECT_EQ(ok->value, 9);

    const auto ng = make_no_default(false);
    EXPECT_FALSE(ng.is_ok());
    EXPECT_EQ(ng.get_message(), "no value");
}

TEST(ResultTest, BoolValueVsFailure) {
    const auto f = flag(0);
    ASSERT_TRUE(f.is_ok()); // false 값이어도 성공
    EXPECT_FALSE(*f);

    const auto t = flag(1);
    ASSERT_TRUE(t.is_ok());
    EXPECT_TRUE(*t);

    const auto e = flag(-1);
    EXPECT_FALSE(e.is_ok());
    EXPECT_EQ(e.get_message(), "invalid");
}

TEST(ResultTest, MoveOnlyValue) {
    auto r = make_ptr();
    ASSERT_TRUE(r.is_ok());
    ASSERT_NE(*r, nullptr);
    EXPECT_EQ(**r, 5);

    std::unique_ptr<int> p = *std::move(r); // operator*() && 로 꺼내기
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 5);
}

TEST(ResultTest, ModifyThroughOperators) {
    auto r = name_literal();
    ASSERT_TRUE(r.is_ok());
    *r += "!";
    r->append("?");
    EXPECT_EQ(*r, "ciao!?");
    EXPECT_EQ(r->size(), 6u);
}

TEST(ResultTest, ConstAccess) {
    const ctls::result<std::string> r = "const";
    ASSERT_TRUE(r.is_ok());
    const std::string& ref = *r;
    EXPECT_EQ(ref, "const");
    EXPECT_EQ(r->size(), 5u);
}

TEST(ResultTest, FromFailedRuntime) {
    const std::string fmt = "rt {}";
    const ctls::result<int> r = ctls::failed_runtime(fmt, 3);
    EXPECT_FALSE(r.is_ok());
    EXPECT_EQ(r.get_message(), "rt 3");
}

TEST(ResultTest, ReturnLocalVariable) {
    const auto ok = return_local(true);
    ASSERT_TRUE(ok.is_ok());
    EXPECT_EQ(*ok, "local");

    const auto ng = return_local(false);
    EXPECT_FALSE(ng.is_ok());
    EXPECT_EQ(ng.get_message(), "local fail");
}

TEST(ResultTest, PropagateToStatus) {
    const auto ng = check(-3);
    EXPECT_FALSE(ng.is_ok());
    EXPECT_EQ(ng.get_message(), "negative: -3");

    const auto ok = check(3);
    EXPECT_TRUE(ok.is_ok());
}

TEST(ResultTest, PropagateToOtherResult) {
    const auto ng = to_str(-5);
    EXPECT_FALSE(ng.is_ok());
    EXPECT_EQ(ng.get_message(), "negative: -5");

    const auto ok = to_str(5);
    ASSERT_TRUE(ok.is_ok());
    EXPECT_EQ(*ok, "10");
}

TEST(ResultTest, IfInitPattern) {
    if (auto r = parse(-1); !r) {
        EXPECT_EQ(r.get_message(), "negative: -1");
    } else {
        FAIL() << "parse(-1) 은 실패해야 함";
    }

    if (auto r = parse(4); r) {
        EXPECT_EQ(*r, 8);
    } else {
        FAIL() << "parse(4) 은 성공해야 함: " << r.get_message();
    }
}

TEST(ResultTest, MoveConstruct) {
    auto a = name_literal();
    const ctls::result<std::string> b{std::move(a)};
    ASSERT_TRUE(b.is_ok());
    EXPECT_EQ(*b, "ciao");
}

TEST(ResultTest, StoreInVector) {
    std::vector<ctls::result<std::string>> v;
    v.push_back("a");
    v.emplace_back("b");
    v.push_back(ctls::failed("e{}", 1));
    for (int i = 0; i < 100; ++i) v.emplace_back("x"); // 재할당 유도 (nothrow move)

    ASSERT_EQ(v.size(), 103u);
    EXPECT_EQ(*v[0], "a");
    EXPECT_EQ(*v[1], "b");
    EXPECT_FALSE(v[2].is_ok());
    EXPECT_EQ(v[2].get_message(), "e1");
    EXPECT_EQ(*v[102], "x");
}

// 컴파일 에러/경고가 나야 정상인 케이스 (참고용)
//   parse(1);                                   // warning: ignoring return value ... 'nodiscard'
//   ctls::result<int> f() { return {}; }        // 값 없는 성공 금지
//   ctls::result<explicit_only> g() { return 3; }  // explicit 생성자만 있는 타입
//   auto c = parse(1); auto d = c;              // 복사 금지
