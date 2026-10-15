#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <utility>

namespace ctls {
    class thread_task {
    public:
        explicit thread_task(
            std::jthread thread,
            std::shared_ptr<std::atomic<bool>> is_done)
            : _thread(std::move(thread)), _is_done(std::move(is_done)) {}

        ~thread_task() = default;

        thread_task(const thread_task&) = delete;
        thread_task& operator=(const thread_task&) = delete;
        thread_task(thread_task&&) = default;
        thread_task& operator=(thread_task&&) = default;

        [[nodiscard]] bool get_is_done() const { return _is_done->load(); }

        [[nodiscard]] bool joinable() const { return _thread.joinable(); }

        // 지금 이 코드를 실행 중인 스레드가 이 task의 스레드인지
        [[nodiscard]] bool is_current_thread() const noexcept {
            return _thread.get_id() == std::this_thread::get_id();
        }

        void join() { _thread.join(); }

        // 자기 자신을 join할 수 없을 때(스레드 안에서 정리하는 경우)만 사용
        void detach() { _thread.detach(); }

        bool request_stop() { return _thread.request_stop(); }

    private:
        std::jthread _thread;
        std::shared_ptr<std::atomic<bool>> _is_done;
    };
} // namespace ctls