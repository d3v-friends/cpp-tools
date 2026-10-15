#pragma once
#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <limits>
#include <ranges>

#include "ctls/debug/logger.hpp"
#include "thread_task.hpp"

namespace ctls {
    class thread_manager {
    public:
        explicit thread_manager() = default;
        ~thread_manager() { stop_all(); }

        thread_manager(const thread_manager&) = delete;
        thread_manager& operator=(const thread_manager&) = delete;
        thread_manager(thread_manager&&) = delete;
        thread_manager& operator=(thread_manager&&) = delete;

        // 전체 정지. 여러 번 불러도 안전하다 (소멸자에서 다시 불려도 에러 아님).
        // 이번 호출이 처음으로 정지를 요청했으면 true.
        bool stop_all() {
            // 락 불필요: stop_source는 동시 호출이 안전하다
            const bool first = _stop_source.request_stop();

            // 락 안에서는 맵을 옮겨 담기만 하고, join은 락 밖에서 한다
            decltype(_threads) local;
            {
                std::scoped_lock lock(_mutex);
                local.swap(_threads);
            }

            // 관리 중인 스레드 안에서 stop_all을 부른 경우: 자기 자신은 join할 수 없다
            for (auto& task : local | std::views::values) {
                if (task.is_current_thread()) {
                    task.detach();
                }
            }

            local.clear(); // jthread 소멸자: request_stop + join
            return first;
        }

        std::stop_token token() const { return _stop_source.get_token(); }

        // 작동 중이면 true. 끝난 task는 여기서 정리한다.
        bool check_running(const std::uint64_t name) {
            decltype(_threads)::node_type node;
            {
                std::scoped_lock lock(_mutex);
                const auto it = _threads.find(name);
                if (it == _threads.end()) {
                    return false;
                }
                if (!it->second.get_is_done()) {
                    return true;
                }
                node = _threads.extract(it);
            }

            return false; // node는 락 밖에서 소멸
        }

        // 끝난 task를 한꺼번에 정리한다.
        void reap() {
            std::map<std::uint64_t, thread_task> finished;
            {
                std::scoped_lock lock(_mutex);
                for (auto it = _threads.begin(); it != _threads.end();) {
                    if (it->second.get_is_done()) {
                        finished.insert(_threads.extract(it++));
                    } else {
                        ++it;
                    }
                }
            }
            finished.clear();
            // finished는 락 밖에서 소멸
        }

        // 개별 정지. 없으면 false.
        // 스레드 안에서 자기 자신을 정지시키면 stop 신호만 보낸다 (join 불가).
        bool stop(const std::uint64_t name) {
            decltype(_threads)::node_type node;
            {
                std::scoped_lock lock(_mutex);
                const auto it = _threads.find(name);
                if (it == _threads.end()) {
                    return false;
                }
                if (it->second.is_current_thread()) {
                    it->second.request_stop();
                    return true; // 맵에 남겨두고, 나중에 reap/stop_all이 정리한다
                }
                node = _threads.extract(it);
            }

            if (auto& task = node.mapped(); task.joinable()) {
                task.request_stop();
                task.join();
            }
            return true;
        }

        // 항상 신규 등록. 실패하면 0 반환.
        std::uint64_t run(std::function<void(std::stop_token)> fn) {
            if (!fn) [[unlikely]] {
                ILOG_ERROR("fn is nullptr");
                return 0;
            }

            reap(); // 끝난 task가 쌓이지 않도록

            std::scoped_lock lock(_mutex);

            // 락 안에서 검사: stop_all의 swap과 순서가 맞물린다
            if (_stop_source.stop_requested()) [[unlikely]] {
                ILOG_ERROR("thread_manager is terminated");
                return 0;
            }

            if (_thread_id == std::numeric_limits<std::uint64_t>::max()) {
                ILOG_ERROR("too many thread");
                return 0;
            }
            _thread_id += 1;

            const auto is_done = std::make_shared<std::atomic<bool>>(false);

            _threads.emplace(
                _thread_id,
                thread_task{
                    std::jthread([global = _stop_source.get_token(), fn = std::move(fn),
                                  is_done](const std::stop_token& own) mutable {
                        // 두 개의 stop 신호 합치기
                        std::stop_source merged;
                        std::stop_callback on_global(
                            global, [&merged] { merged.request_stop(); });
                        std::stop_callback on_own(
                            own, [&merged] { merged.request_stop(); });

                        try {
                            fn(merged.get_token());
                        } catch (const std::exception& err) {
                            ILOG_ERROR("err: error={}", err.what());
                        } catch (...) {
                            ILOG_ERROR("error");
                        }

                        is_done->store(true);
                    }),
                    is_done});

            return _thread_id;
        }

    private:
        mutable std::mutex _mutex;
        std::map<std::uint64_t, thread_task> _threads;
        std::stop_source _stop_source;
        std::uint64_t _thread_id = 0;
    };
} // namespace ctls
