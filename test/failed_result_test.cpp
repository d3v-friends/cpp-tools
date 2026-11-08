#include <gtest/gtest.h>

#include <ctls/error/failed_result.hpp>

#include <format>       // std::format_error
#include <string>       // std::string
#include <string_view>  // std::string_view
#include <type_traits>  // std::is_aggregate_v

// [2026-11-08] ctls::failed / ctls::failed_runtime 테스트

static_assert(std::is_aggregate_v<ctls::failed_result>);

TEST(FailedResultTest, PlainMessage) {
    const auto f = ctls::failed(std::string{"db open failed"});
    EXPECT_EQ(f.message, "db open failed");
}

TEST(FailedResultTest, LvalueString) {
    const std::string msg = "lvalue message";
    const auto f = ctls::failed(msg);
    EXPECT_EQ(f.message, "lvalue message");
    EXPECT_EQ(msg, "lvalue message"); // 원본은 그대로
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
    const auto f = ctls::failed("{}", other); // 런타임 문자열을 그대로 넘기기
    EXPECT_EQ(f.message, "inner error {}");
}

TEST(FailedResultTest, RuntimeFormat) {
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

// 컴파일 에러가 나야 정상인 케이스 (gtest 로 자동 검증 불가, 참고용)
//   ctls::failed("{} {}", 1);          // 인자 개수 불일치 → consteval 에러
//   std::string fmt = "{}";
//   ctls::failed(fmt, 1);              // 런타임 포맷 문자열 → failed_runtime 사용
