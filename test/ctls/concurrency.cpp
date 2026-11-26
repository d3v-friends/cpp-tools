#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include "ctls/ctls.hpp"

using namespace std::chrono_literals;

namespace {
    using steady = std::chrono::steady_clock;

    double elapsed_ms(steady::time_point from, steady::time_point to = steady::now()) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    }

    // wait 상태가 끝날 때까지 try_receive 반복 (timeout 넘으면 그대로 반환)
    template <class T>
    std::pair<ctls::e_channel_status, std::optional<T>>
    try_receive_until_ready(ctls::tps_channel<T>& ch, std::chrono::milliseconds timeout = 2000ms) {
        const auto deadline = steady::now() + timeout;
        while (true) {
            auto result = ch.try_receive();
            if (result.first != ctls::e_channel_status::wait || steady::now() > deadline) {
                return result;
            }
            std::this_thread::sleep_for(1ms);
        }
    }
} // namespace

// test 이름은 PascalCase 로 한다.
// _ 언더바는 권장하지 않음

TEST(TpsChannel, SendReceiveFifo) {
    ctls::tps_channel<int> ch{1000.0f};
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(ch.send(i));
    }
    for (int i = 0; i < 5; ++i) {
        auto value = ch.receive();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i);
    }
}

TEST(TpsChannel, ReceiveRespectsTps) {
    constexpr float tps = 20.0f;            // 50ms 간격
    constexpr int count = 6;
    constexpr double interval_ms = 1000.0 / tps;

    ctls::tps_channel<int> ch{tps};
    for (int i = 0; i < count; ++i) {
        ASSERT_TRUE(ch.send(i));
    }

    // 첫 값은 생성 시점 정책에 따라 달라서 기준점으로만 사용
    ASSERT_TRUE(ch.receive().has_value());
    const auto start = steady::now();
    for (int i = 1; i < count; ++i) {
        ASSERT_TRUE(ch.receive().has_value());
    }

    const double elapsed = elapsed_ms(start);
    const double expected = (count - 1) * interval_ms;
    ILOG_TRACE("elapsed={}ms expected={}ms", elapsed, expected);
    EXPECT_GE(elapsed, expected * 0.9);
    EXPECT_LT(elapsed, expected * 1.5);
}

TEST(TpsChannel, TryReceiveRespectsTps) {
    ctls::tps_channel<int> ch{10.0f};       // 100ms 간격
    ASSERT_TRUE(ch.send(1));
    ASSERT_TRUE(ch.send(2));

    auto first = try_receive_until_ready(ch);
    ASSERT_EQ(first.first, ctls::e_channel_status::ok);
    EXPECT_EQ(first.second.value(), 1);

    // 바로 다시 꺼내면 아직 시간이 안 돼서 wait 이어야 함
    auto second = ch.try_receive();
    EXPECT_EQ(second.first, ctls::e_channel_status::wait);
    EXPECT_FALSE(second.second.has_value());

    std::this_thread::sleep_for(110ms);
    auto third = ch.try_receive();
    ASSERT_EQ(third.first, ctls::e_channel_status::ok);
    EXPECT_EQ(third.second.value(), 2);
}

TEST(TpsChannel, TryReceiveEmpty) {
    ctls::tps_channel<int> ch{1000.0f};
    std::this_thread::sleep_for(5ms);

    auto [status, value] = ch.try_receive();
    EXPECT_EQ(status, ctls::e_channel_status::empty);
    EXPECT_FALSE(value.has_value());
}

TEST(TpsChannel, TryReceiveDrainsBeforeClosed) {
    ctls::tps_channel<int> ch{10.0f};
    ASSERT_TRUE(ch.send(1));
    ASSERT_TRUE(ch.send(2));
    ch.close();

    // 남은 값이 있는 동안은 closed 가 나오면 안 됨
    std::vector<int> received;
    while (true) {
        auto [status, value] = try_receive_until_ready(ch);
        if (status == ctls::e_channel_status::ok) {
            received.push_back(value.value());
            continue;
        }
        EXPECT_EQ(status, ctls::e_channel_status::closed);
        break;
    }
    EXPECT_EQ(received, (std::vector<int>{1, 2}));
}

TEST(TpsChannel, ReceiveDrainsAfterClose) {
    ctls::tps_channel<int> ch{1000.0f};
    ASSERT_TRUE(ch.send(1));
    ch.close();

    auto value = ch.receive();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);

    EXPECT_FALSE(ch.receive().has_value());
}

TEST(TpsChannel, SendAfterCloseFails) {
    ctls::tps_channel<int> ch{1000.0f};
    ch.close();
    EXPECT_TRUE(ch.is_closed());
    EXPECT_FALSE(ch.send(1));
}

TEST(TpsChannel, SendBlocksWhenFull) {
    ctls::tps_channel<int> ch{1000.0f, 1};
    ASSERT_TRUE(ch.send(1));

    std::atomic<bool> sent{false};
    std::jthread producer{[&] {
        ch.send(2);
        sent = true;
    }};

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(sent.load());               // 꽉 차서 대기 중이어야 함

    auto value = ch.receive();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);

    const auto deadline = steady::now() + 1s;
    while (!sent.load() && steady::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_TRUE(sent.load());
}

TEST(TpsChannel, CloseWakesBlockedReceive) {
    ctls::tps_channel<int> ch{1000.0f};
    std::atomic<bool> done{false};
    std::atomic<bool> returned_empty{false};

    std::jthread consumer{[&] {
        returned_empty = !ch.receive().has_value();
        done = true;
    }};

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(done.load());

    ch.close();
    consumer.join();
    EXPECT_TRUE(returned_empty.load());
}

TEST(TpsChannel, CloseWakesBlockedSend) {
    ctls::tps_channel<int> ch{1000.0f, 1};
    ASSERT_TRUE(ch.send(1));
    std::atomic<bool> send_result{true};

    std::jthread producer{[&] { send_result = ch.send(2); }};

    std::this_thread::sleep_for(50ms);
    ch.close();
    producer.join();
    EXPECT_FALSE(send_result.load());
}

TEST(TpsChannel, MultipleConsumersRespectTps) {
    constexpr float tps = 20.0f;            // 50ms 간격
    constexpr int count = 9;
    constexpr double interval_ms = 1000.0 / tps;

    ctls::tps_channel<int> ch{tps};
    for (int i = 0; i < count; ++i) {
        ASSERT_TRUE(ch.send(i));
    }
    ch.close();                              // 다 꺼내면 receive 가 nullopt 로 끝나도록

    std::mutex mutex;
    std::vector<steady::time_point> times;
    {
        std::vector<std::jthread> consumers;
        for (int i = 0; i < 3; ++i) {
            consumers.emplace_back([&] {
                while (ch.receive().has_value()) {
                    std::scoped_lock lock{mutex};
                    times.push_back(steady::now());
                }
            });
        }
    } // 여기서 join

    ASSERT_EQ(times.size(), static_cast<std::size_t>(count));
    std::ranges::sort(times);

    const double elapsed = elapsed_ms(times.front(), times.back());
    const double expected = (count - 1) * interval_ms;
    ILOG_INFO("consumers=3 elapsed={}ms expected={}ms", elapsed, expected);
    EXPECT_GE(elapsed, expected * 0.9);
}

TEST(TpsChannel, SetTpsChangesRate) {
    ctls::tps_channel<int> ch{1000.0f};
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(ch.send(i));
    }
    ASSERT_TRUE(ch.receive().has_value());

    ch.set_tps(20.0f);                       // 50ms 간격으로 변경
    const auto start = steady::now();
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(ch.receive().has_value());
    }

    const double elapsed = elapsed_ms(start);
    ILOG_INFO("elapsed={}ms", elapsed);
    EXPECT_GE(elapsed, 150.0 * 0.9);
}

// tps <= 0 검증(throw)을 추가한 경우에만 통과
TEST(TpsChannel, InvalidTpsThrows) {
    EXPECT_DEATH({ ctls::tps_channel<int> ch{0.0f}; }, "");
    EXPECT_DEATH({ ctls::tps_channel<int> ch{-1.0f}; }, "");
}