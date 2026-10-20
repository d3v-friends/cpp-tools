#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <cstdint>
#include <chrono>
#include <thread>

#include "types.hpp"
#include "ctls/debug/assert.hpp"

namespace ctls {
    // go 의 chan 과 비슷하게 동작하는 스레드 안전 큐
    // capacity == 0 이면 크기 제한 없음
    // tps 속도에 맞춰 receive 할수 있다

    template <class T> class tps_channel {
    public:
        explicit tps_channel(const float tps, const std::size_t capacity = 0)
            : _capacity{capacity}, _last_pop_at{std::chrono::steady_clock::now()},
              _tps{tps}, _delay_ms{1000.0f / tps} {
            IASSERT(0.0f < tps, "tps must be positive");
        }
        ~tps_channel() = default;
        tps_channel(const tps_channel&) = delete;
        tps_channel& operator=(const tps_channel&) = delete;
        tps_channel(tps_channel&&) = delete;
        tps_channel& operator=(tps_channel&&) = delete;

        void set_tps(const float tps) {
            IASSERT(0.0f < tps, "tps must be positive");
            std::scoped_lock lock{_mutex};
            _tps = tps;
            _delay_ms = 1000.0f / tps;
        }

        // ch <- value. 가득 차 있으면 자리가 날 때까지 기다림. 닫혀 있으면 false
        bool send(T value) {
            std::unique_lock lock{_mutex};
            _not_full.wait(lock, [this] { return _is_closed || !is_full_unlocked(); });
            if (_is_closed) {
                return false;
            }
            _queue.push_back(std::move(value));
            lock.unlock();
            _not_empty.notify_one();
            return true;
        }

        // value, ok := <-ch. 비어 있으면 올 때까지 기다림. 닫히고 비었으면 nullopt
        std::optional<T> receive() {
            std::unique_lock lock{_mutex};
            _not_empty.wait(lock, [this] { return _is_closed || !_queue.empty(); });
            if (_queue.empty()) {
                return std::nullopt; // 닫히고 비었으면 바로 종료
            }

            // 꺼낼 시각을 lock 안에서 예약
            const auto now = std::chrono::steady_clock::now();
            auto next = _last_pop_at + interval_unlocked();
            if (next < now) {
                next = now;
            }
            _last_pop_at = next;

            T value = std::move(_queue.front());
            _queue.pop_front();
            lock.unlock();
            _not_full.notify_one();

            std::this_thread::sleep_until(next);
            return value;
        }

        // select { case v := <-ch: ... default: ... }
        std::pair<e_channel_status, std::optional<T>> try_receive() {
            std::unique_lock lock{_mutex};
            if (_queue.empty()) {
                return {
                    _is_closed ? e_channel_status::closed : e_channel_status::empty,
                    std::nullopt};
            }

            const auto now = std::chrono::steady_clock::now();
            if (now < _last_pop_at + interval_unlocked()) {
                return {
                    e_channel_status::wait, std::nullopt}; // 값은 있는데 아직 시간 안 됨
            }

            _last_pop_at = now; // 갱신
            T value = std::move(_queue.front());
            _queue.pop_front();
            lock.unlock();
            _not_full.notify_one();
            return {e_channel_status::ok, std::move(value)};
        }

        // close(ch). 기다리던 send/receive 를 모두 깨움
        void close() {
            {
                std::scoped_lock lock{_mutex};
                _is_closed = true;
            }
            _not_empty.notify_all();
            _not_full.notify_all();
        }

        [[nodiscard]] bool is_closed() const {
            std::scoped_lock lock{_mutex};
            return _is_closed;
        }

        [[nodiscard]] std::size_t size() const {
            std::scoped_lock lock{_mutex};
            return _queue.size();
        }

        [[nodiscard]] bool empty() const {
            std::scoped_lock lock{_mutex};
            return _queue.empty();
        }

        std::size_t flush() {
            std::deque<T> drained;
            {
                std::scoped_lock lock{_mutex};
                drained.swap(_queue);
                _is_closed = true;
            }
            _not_empty.notify_all();
            _not_full.notify_all();
            return drained.size(); // drained는 락 밖에서 소멸
        }

    private:
        mutable std::mutex _mutex;
        std::condition_variable _not_empty;
        std::condition_variable _not_full;
        std::deque<T> _queue;
        std::size_t _capacity;
        std::chrono::steady_clock::time_point _last_pop_at;
        float _tps;
        float _delay_ms;
        bool _is_closed = false;

    private:
        std::chrono::steady_clock::duration interval_unlocked() const {
            return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<float, std::milli>(_delay_ms));
        }

        [[nodiscard]] bool is_full_unlocked() const {
            return _capacity != 0 && _queue.size() >= _capacity;
        }
    };

} // namespace ctls
