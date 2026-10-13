#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <cstdint>

namespace ctls {

    enum class e_channel_status : std::uint8_t {
        ok,     // 값 있음
        empty,  // 아직 안 닫혔는데 비어 있음
        closed, // 닫혔고 남은 것도 없음
    };

    // go 의 chan 과 비슷하게 동작하는 스레드 안전 큐
    // capacity == 0 이면 크기 제한 없음
    template <class T> class channel {
    public:
        explicit channel(const std::size_t capacity = 0) : _capacity{capacity} {}
        channel(const channel&) = delete;
        channel& operator=(const channel&) = delete;

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
            return pop_unlocked(lock);
        }

        // select { case v := <-ch: ... default: ... }
        std::pair<e_channel_status, std::optional<T>> try_receive() {
            std::unique_lock lock{_mutex};
            if (auto value = pop_unlocked(lock)) {
                return {e_channel_status::ok, std::move(value)};
            }

            // 값이 없으면 락이 안풀려서 여기도 락인 상태로 된다.
            const auto status =
                _is_closed ? e_channel_status::closed : e_channel_status::empty;
            return {status, std::nullopt};
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

    private:
        mutable std::mutex _mutex;
        std::condition_variable _not_empty;
        std::condition_variable _not_full;
        std::deque<T> _queue;
        std::size_t _capacity;
        bool _is_closed = false;

        [[nodiscard]] bool is_full_unlocked() const {
            return _capacity != 0 && _queue.size() >= _capacity;
        }

        std::optional<T> pop_unlocked(std::unique_lock<std::mutex>& lock) {
            if (_queue.empty()) {
                return std::nullopt;
            }
            T value = std::move(_queue.front());
            _queue.pop_front();
            lock.unlock();
            _not_full.notify_one();
            return value;
        }
    };
} // namespace ctls