#pragma once

#include <atomic>
#include <thread>
#include <memory>

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

        std::jthread& get_thread() { return _thread; }

        [[nodiscard]] bool joinable() const { return _thread.joinable(); }

        void join() { _thread.join(); }

        bool request_stop() { return _thread.request_stop(); }

    private:
        std::jthread _thread;
        std::shared_ptr<std::atomic<bool>> _is_done;
    };
} // namespace ctls::thread