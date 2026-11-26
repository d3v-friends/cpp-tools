#include <gtest/gtest.h>

#include <ctls/error/failed_result.hpp>

#include <chrono>           // std::chrono::system_clock
#include <cstdint>          // std::uint_least32_t
#include <format>           // std::format_error
#include <source_location>  // std::source_location
#include <string>           // std::string, std::to_string
#include <string_view>      // std::string_view
#include <type_traits>      // std::is_*_v
#include <utility>          // std::move, std::forward
#include <vector>           // std::vector

// [2026-11-13] ctls::failed_result 테스트
// failed / failed_runtime (message, location, timestamp) + before 체인 (append_before, fails)

namespace {
    using clock_type = std::chrono::system_clock;

    // function_name() 확인용. TEST 매크로 함수 이름은 gtest 버전마다 달라서 헬퍼로 고정
    ctls::failed_result failed_plain_in_helper() { return ctls::failed(std::string{"helper"}); }
    ctls::failed_result failed_fmt_in_helper(int code) { return ctls::failed("helper {}", code); }
    ctls::failed_result failed_rt_in_helper(const std::string& fmt) {
        return ctls::failed_runtime(fmt, 1);
    }

    // 파일 이름을 직접 적지 않고 컴파일러가 알려주는 현재 파일 경로와 비교 (파일 이름이 바뀌어도 안 깨짐)
    bool is_this_file(const std::source_location& loc) {
        return std::string_view{loc.file_name()} == std::source_location::current().file_name();
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

    // 체인을 앞에서부터 따라가며 message 를 모은다
    std::vector<std::string> chain_messages(const ctls::failed_result& f) {
        std::vector<std::string> out;
        for (const ctls::failed_result* p = &f; p != nullptr; p = p->before.get()) {
            out.push_back(p->message);
        }
        return out;
    }

    // fails(args...) 를 부를 수 있는지 컴파일 타임에 확인
    template <class... A>
    concept can_fails = requires(A&&... a) { ctls::fails(std::forward<A>(a)...); };

    // obj.append_before(arg) 를 부를 수 있는지 컴파일 타임에 확인
    template <class Obj, class Arg>
    concept can_append = requires(Obj&& obj, Arg&& arg) {
        std::forward<Obj>(obj).append_before(std::forward<Arg>(arg));
    };
} // namespace

// ---------------------------------------------------------------- 컴파일 타임 규칙

static_assert(std::is_aggregate_v<ctls::failed_result>);
static_assert(std::is_same_v<decltype(ctls::failed_result::location), std::source_location>);
static_assert(std::is_same_v<decltype(ctls::failed_result::timestamp), clock_type::time_point>);

// before 가 unique_ptr 라서 복사는 안 되고 이동만 된다
static_assert(!std::is_copy_constructible_v<ctls::failed_result>);
static_assert(!std::is_copy_assignable_v<ctls::failed_result>);
static_assert(std::is_move_constructible_v<ctls::failed_result>);
static_assert(std::is_move_assignable_v<ctls::failed_result>);

// append_before 는 rvalue 만 받고, 값을 바꾸므로 const 객체에서는 부를 수 없다
static_assert(can_append<ctls::failed_result&, ctls::failed_result>);
static_assert(!can_append<ctls::failed_result&, ctls::failed_result&>);       // std::move 필요
static_assert(!can_append<ctls::failed_result&, const ctls::failed_result&>);
static_assert(!can_append<const ctls::failed_result&, ctls::failed_result>);  // const 아님

// fails 는 failed_result 를 1개 이상, rvalue 로만 받는다
static_assert(can_fails<ctls::failed_result>);
static_assert(can_fails<ctls::failed_result, ctls::failed_result, ctls::failed_result>);
static_assert(!can_fails<>);                                      // 인자 0개
static_assert(!can_fails<ctls::failed_result&>);                  // lvalue → std::move 필요
static_assert(!can_fails<ctls::failed_result, ctls::failed_result&>);
static_assert(!can_fails<ctls::failed_result, const ctls::failed_result&>);
static_assert(!can_fails<ctls::failed_result, int>);
static_assert(!can_fails<ctls::failed_result, std::string>);
static_assert(std::is_same_v<
    decltype(ctls::fails(ctls::failed_result{}, ctls::failed_result{})), ctls::failed_result>);

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

TEST(FailedResultTest, LocationOfPlainString) {
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed(std::string{"where"});
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(is_this_file(f.location));
}

TEST(FailedResultTest, LocationOfFormat) {
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed("where {}", 1);
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(is_this_file(f.location));
}

TEST(FailedResultTest, LocationOfRuntime) {
    const std::string fmt = "where {}";
    const std::uint_least32_t line = __LINE__ + 1;
    const auto f = ctls::failed_runtime(fmt, 1);
    EXPECT_EQ(f.location.line(), line);
    EXPECT_TRUE(is_this_file(f.location));
}

TEST(FailedResultTest, LocationFunctionNameIsCaller) {
    // 위치는 failed 를 부른 함수 안이어야 한다 (failed 내부가 아님)
    EXPECT_TRUE(contains(failed_plain_in_helper().location.function_name(), "failed_plain_in_helper"));
    EXPECT_TRUE(contains(failed_fmt_in_helper(1).location.function_name(), "failed_fmt_in_helper"));
    EXPECT_TRUE(contains(failed_rt_in_helper("{}").location.function_name(), "failed_rt_in_helper"));
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

// ---------------------------------------------------------------- before (기본값 / 이동)

TEST(FailedResultTest, NewFailedHasNoBefore) {
    EXPECT_EQ(ctls::failed(std::string{"a"}).before, nullptr);
    EXPECT_EQ(ctls::failed("a {}", 1).before, nullptr);
    EXPECT_EQ(ctls::failed_runtime("a").before, nullptr);
}

TEST(FailedResultTest, MoveKeepsChain) {
    auto a = ctls::failed("a");
    a.append_before(ctls::failed("b"));
    const auto moved = std::move(a);
    EXPECT_EQ(chain_messages(moved), (std::vector<std::string>{"a", "b"}));
}

TEST(FailedResultTest, MoveAssignReplacesChain) {
    auto a = ctls::failed("a");
    a.append_before(ctls::failed("a2"));
    auto b = ctls::failed("b");
    b.append_before(ctls::failed("b2"));

    a = std::move(b); // 기존 a 체인은 해제되고 b 체인으로 바뀐다
    EXPECT_EQ(chain_messages(a), (std::vector<std::string>{"b", "b2"}));
}

// ---------------------------------------------------------------- append_before

TEST(FailedResultTest, AppendBeforeToEmpty) {
    auto a = ctls::failed("a");
    a.append_before(ctls::failed("b"));
    ASSERT_NE(a.before, nullptr);
    EXPECT_EQ(a.before->message, "b");
    EXPECT_EQ(a.before->before, nullptr);
}

TEST(FailedResultTest, AppendBeforeGoesToTail) {
    auto a = ctls::failed("a");
    a.append_before(ctls::failed("b"));
    a.append_before(ctls::failed("c"));
    a.append_before(ctls::failed("d"));
    EXPECT_EQ(chain_messages(a), (std::vector<std::string>{"a", "b", "c", "d"}));
}

TEST(FailedResultTest, AppendBeforeKeepsChainOfValue) {
    // 이미 체인이 있는 값을 붙이면 그 체인째로 끝에 붙는다
    auto b = ctls::failed("b");
    b.append_before(ctls::failed("b2"));

    auto a = ctls::failed("a");
    a.append_before(std::move(b));
    a.append_before(ctls::failed("c"));
    EXPECT_EQ(chain_messages(a), (std::vector<std::string>{"a", "b", "b2", "c"}));
}

TEST(FailedResultTest, AppendBeforeKeepsLocationAndTimestamp) {
    const std::uint_least32_t line = __LINE__ + 1;
    auto b = ctls::failed("b");
    const auto ts = b.timestamp;

    auto a = ctls::failed("a");
    a.append_before(std::move(b));
    ASSERT_NE(a.before, nullptr);
    EXPECT_EQ(a.before->location.line(), line);
    EXPECT_EQ(a.before->timestamp, ts);
}

TEST(FailedResultTest, AppendBeforeDoesNotChangeHead) {
    const std::uint_least32_t line = __LINE__ + 1;
    auto a = ctls::failed("a");
    const auto ts = a.timestamp;
    a.append_before(ctls::failed("b"));
    EXPECT_EQ(a.message, "a");
    EXPECT_EQ(a.location.line(), line);
    EXPECT_EQ(a.timestamp, ts);
}

TEST(FailedResultTest, AppendBeforeLongChain) {
    // 체인이 길어도 끝을 정확히 찾는지 (붙일 때마다 끝까지 따라감)
    auto head = ctls::failed("0");
    for (int i = 1; i < 100; ++i) {
        head.append_before(ctls::failed(std::to_string(i)));
    }
    const auto msgs = chain_messages(head);
    ASSERT_EQ(msgs.size(), 100U);
    EXPECT_EQ(msgs.front(), "0");
    EXPECT_EQ(msgs[50], "50");
    EXPECT_EQ(msgs.back(), "99");
}

// ---------------------------------------------------------------- fails

TEST(FailedResultTest, FailsSingleReturnsFirst) {
    const auto f = ctls::fails(ctls::failed("e1"));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"e1"}));
    EXPECT_EQ(f.before, nullptr);
}

TEST(FailedResultTest, FailsTwo) {
    const auto f = ctls::fails(ctls::failed("e1"), ctls::failed("e2"));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"e1", "e2"}));
}

TEST(FailedResultTest, FailsKeepsInputOrder) {
    const auto f = ctls::fails(
        ctls::failed("e1"), ctls::failed("e2"), ctls::failed("e3"), ctls::failed("e4"));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"e1", "e2", "e3", "e4"}));
}

TEST(FailedResultTest, FailsAcceptsMovedLvalues) {
    auto a = ctls::failed("a");
    auto b = ctls::failed("b");
    const auto f = ctls::fails(std::move(a), std::move(b));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"a", "b"}));
}

TEST(FailedResultTest, FailsMixedFactories) {
    const std::string fmt = "rt {}";
    const auto f = ctls::fails(
        ctls::failed(std::string{"plain"}), ctls::failed("fmt {}", 1), ctls::failed_runtime(fmt, 2));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"plain", "fmt 1", "rt 2"}));
}

TEST(FailedResultTest, FailsJoinsExistingChains) {
    auto a = ctls::fails(ctls::failed("a1"), ctls::failed("a2"));
    auto b = ctls::fails(ctls::failed("b1"), ctls::failed("b2"));
    const auto f = ctls::fails(std::move(a), std::move(b), ctls::failed("c"));
    EXPECT_EQ(chain_messages(f), (std::vector<std::string>{"a1", "a2", "b1", "b2", "c"}));
}

TEST(FailedResultTest, FailsKeepsEachLocation) {
    const std::uint_least32_t line1 = __LINE__ + 1;
    auto e1 = ctls::failed("e1");
    const std::uint_least32_t line2 = __LINE__ + 1;
    auto e2 = ctls::failed("e2");

    const auto f = ctls::fails(std::move(e1), std::move(e2));
    ASSERT_NE(f.before, nullptr);
    EXPECT_EQ(f.location.line(), line1);
    EXPECT_EQ(f.before->location.line(), line2);
}

TEST(FailedResultTest, FailsKeepsEachTimestamp) {
    auto e1 = ctls::failed("e1");
    auto e2 = ctls::failed("e2");
    const auto t1 = e1.timestamp;
    const auto t2 = e2.timestamp;

    const auto f = ctls::fails(std::move(e1), std::move(e2));
    ASSERT_NE(f.before, nullptr);
    EXPECT_EQ(f.timestamp, t1);
    EXPECT_EQ(f.before->timestamp, t2);
}

// 컴파일 에러가 나야 정상인 케이스 (gtest 로 자동 검증 불가, 참고용)
//   ctls::failed("{} {}", 1);          // 인자 개수 불일치 → consteval 에러
//   std::string fmt = "{}";
//   ctls::failed(fmt, 1);              // 런타임 포맷 문자열 → failed_runtime 사용
// append_before / fails 의 인자 규칙은 위 static_assert(can_append, can_fails) 로 검증한다
