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
            : m_thread(std::move(thread)), m_is_done(std::move(is_done)) {}

        ~thread_task() = default;

        thread_task(const thread_task&) = delete;
        thread_task& operator=(const thread_task&) = delete;
        thread_task(thread_task&&) = default;
        thread_task& operator=(thread_task&&) = default;

        [[nodiscard]] bool get_is_done() const { return m_is_done->load(); }

        std::jthread& get_thread() { return m_thread; }

        [[nodiscard]] bool joinable() const { return m_thread.joinable(); }

        void join() { m_thread.join(); }

        bool request_stop() { return m_thread.request_stop(); }

    private:
        std::jthread m_thread;
        std::shared_ptr<std::atomic<bool>> m_is_done;
    };
} // namespace ctls::thread