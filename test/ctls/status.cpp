#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "ctls/error/fn.hpp"

namespace {
    using ctls::failed;
    using ctls::failed_result;
    using ctls::make_statuses;
    using ctls::ok;
    using ctls::status;
    using ::testing::ElementsAre;
    using ::testing::IsEmpty;

    // 체인을 앞에서부터 따라가며 메세지를 모은다 (실패한 것만)
    std::vector<std::string> collect(const status& s) {
        std::vector<std::string> out;
        for (const status* p = &s; p != nullptr; p = p->get_chain()) {
            if (!p->is_ok()) {
                out.push_back(p->get_message());
            }
        }
        return out;
    }

    // make_statuses 호출 가능 여부 (lvalue status 차단 확인용)
    template <typename... Ts>
    concept can_make_statuses = requires(Ts&&... ts) {
        make_statuses(std::forward<Ts>(ts)...);
    };
} // namespace

// 인자 없음 → ok
TEST(make_statuses, no_args_returns_ok) {
    auto s = make_statuses();
    EXPECT_TRUE(s.is_ok());
    EXPECT_TRUE(static_cast<bool>(s));
    EXPECT_EQ(s.get_chain(), nullptr);
}

// 전부 성공 → ok
TEST(make_statuses, all_ok_returns_ok) {
    auto s = make_statuses(ok(), ok(), status::ok());
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(s.get_chain(), nullptr);
    EXPECT_THAT(collect(s), IsEmpty());
}

// failed_result 하나
TEST(make_statuses, single_failed_result) {
    auto s = make_statuses(failed("e1"));
    EXPECT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "e1");
    EXPECT_EQ(s.get_chain(), nullptr);
}

// 실패한 status 하나
TEST(make_statuses, single_failed_status) {
    status e = failed("e1");
    auto s = make_statuses(std::move(e));
    EXPECT_FALSE(s.is_ok());
    EXPECT_EQ(s.get_message(), "e1");
    EXPECT_EQ(s.get_chain(), nullptr);
}

// 실패만 역순으로 묶인다: (e1, ok, e2, ok, e3) → e3 → e2 → e1
TEST(make_statuses, failed_only_in_reverse_order) {
    auto s = make_statuses(
        failed("e1"),
        ok(),
        status{failed("e2")},
        ok(),
        failed("e3"));

    EXPECT_FALSE(s.is_ok());
    EXPECT_THAT(collect(s), ElementsAre("e3", "e2", "e1"));

    // 체인 개수 = 실패 개수 (ok 는 체인에 들어가지 않음)
    int count = 0;
    for (const status* p = &s; p != nullptr; p = p->get_chain()) {
        EXPECT_FALSE(p->is_ok());
        ++count;
    }
    EXPECT_EQ(count, 3);
}

// 실패 앞뒤로 ok 가 있어도 실패 하나만 남는다
TEST(make_statuses, ok_around_single_failure) {
    auto s = make_statuses(ok(), failed("only"), ok());
    EXPECT_FALSE(s.is_ok());
    EXPECT_THAT(collect(s), ElementsAre("only"));
}

// 이미 체인을 가진 status 를 넣으면 기존 체인이 유지된다
// a = (a1, a2) → a2 → a1
// make_statuses(b1, a, b2) → b2 → a2 → a1 → b1
TEST(make_statuses, keeps_existing_chain) {
    auto a = make_statuses(failed("a1"), failed("a2"));
    ASSERT_THAT(collect(a), ElementsAre("a2", "a1"));

    auto s = make_statuses(failed("b1"), std::move(a), failed("b2"));
    EXPECT_THAT(collect(s), ElementsAre("b2", "a2", "a1", "b1"));
}

// 결과를 다시 make_statuses 에 넣어도 된다 (중첩)
TEST(make_statuses, nested) {
    auto s = make_statuses(
        make_statuses(failed("x1"), failed("x2")),
        make_statuses(ok(), ok()),
        failed("y"));
    EXPECT_THAT(collect(s), ElementsAre("y", "x2", "x1"));
}

// failed_result lvalue 는 복사되어 원본이 남는다
TEST(make_statuses, lvalue_failed_result_is_copied) {
    failed_result f = failed("copy me");
    auto s = make_statuses(f);
    EXPECT_EQ(s.get_message(), "copy me");
    EXPECT_EQ(f.message, "copy me");
}

// location, timestamp 가 그대로 전달된다
TEST(make_statuses, keeps_location_and_timestamp) {
    failed_result f = failed("loc");
    const auto line = f.location.line();
    const auto ts = f.timestamp;

    auto s = make_statuses(ok(), std::move(f));
    EXPECT_EQ(s.get_location().line(), line);
    EXPECT_EQ(s.get_timestamp(), ts);
}

// to_failed 로 꺼낸 값도 섞어서 받을 수 있다
TEST(make_statuses, accepts_to_failed) {
    status e = failed("from status");
    auto s = make_statuses(e.to_failed(), failed("next"));
    EXPECT_THAT(collect(s), ElementsAre("next", "from status"));
    EXPECT_EQ(e.get_message(), "from status"); // const& 버전은 원본 유지
}

// 컴파일 타임 체크
// status lvalue 는 복사가 막혀 있으므로 받으면 안 된다 (std::move 필요)
static_assert(can_make_statuses<>);
static_assert(can_make_statuses<status>);
static_assert(can_make_statuses<failed_result>);
static_assert(can_make_statuses<failed_result&>);
static_assert(can_make_statuses<const failed_result&>);
static_assert(can_make_statuses<status, failed_result, failed_result&>);
static_assert(!can_make_statuses<status&>);
static_assert(!can_make_statuses<const status&>);
static_assert(!can_make_statuses<int>);
static_assert(!can_make_statuses<std::string>);
