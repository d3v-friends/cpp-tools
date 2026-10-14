#pragma once

#include <thread>
#include <mutex>
#include <map>
#include <memory>
#include <atomic>
#include <string>
#include <functional>
#include <stop_token>
#include <exception>

#include "thread_task.hpp"
#include "../debug/logger.hpp"

namespace ctls {
    class thread_manager {
    public:
        explicit thread_manager() = default;
        ~thread_manager() { stop_all(); }

        // 복사금지
        thread_manager(const thread_manager&) = delete;
        thread_manager& operator=(const thread_manager&) = delete;
        thread_manager(thread_manager&&) = delete;
        thread_manager& operator=(thread_manager&&) = delete;

        void stop_all() {
            _is_terminated.store(true);
            std::scoped_lock lock(_mutex);
            _stop_source.request_stop();
            _threads.clear();
        }

        std::stop_token token() const { return _stop_source.get_token(); }

        bool check_running(const std::string& name) {
            std::scoped_lock lock(_mutex);
            return check_running_unlocked(name);
        }

        bool stop(const std::string& name) {
            decltype(_threads)::node_type node;
            {
                std::scoped_lock lock(_mutex);
                node = _threads.extract(name);
            }

            if (node.empty()) {
                return false;
            }

            if (auto& task = node.mapped(); task.joinable()) {
                task.request_stop();
                task.join();
            }

            return true;
        }

        // 신규로 등록이 되면 true
        // 기존값이 있으면 false
        bool run(const std::string& name, std::function<void(std::stop_token)> fn) {
            if (!fn) [[unlikely]] {
                I_LOG("fn is nullptr");
                return false;
            }

            if (_is_terminated.load()) [[unlikely]] {
                I_LOG("thread_manager is terminated");
                return false;
            }

            std::scoped_lock lock(_mutex);
            if (check_running_unlocked(name)) {
                return false;
            }

            const auto is_done = std::make_shared<std::atomic<bool>>(false);
            auto t = thread_task{
                std::jthread([global = _stop_source.get_token(), fn = std::move(fn),
                              is_done](const std::stop_token& own) mutable {
                    // 두개의 stop 신호 정보 합치기
                    std::stop_source merged;
                    std::stop_callback on_global(
                        global, [&merged] { merged.request_stop(); });
                    std::stop_callback on_own(own, [&merged] { merged.request_stop(); });

                    // thread 실행
                    try {
                        fn(merged.get_token());
                    } catch (const std::exception& err) {
                        I_LOG_F("err: error={}", err.what());
                    } catch (...) {
                        // todo 이 부분 어떻게 할지 나중에 정하기
                    }

                    // 종료
                    is_done->store(true);
                }),
                is_done};

            // 추가 실패시 스레드 종료하고 실패로 결과값 리턴
            return _threads.try_emplace(name, std::move(t)).second;
        }

    private:
        mutable std::mutex _mutex;
        std::map<std::string, thread_task> _threads{};
        std::stop_source _stop_source;
        std::atomic<bool> _is_terminated{false};

        bool check_running_unlocked(const std::string& name) {
            if (const auto task = _threads.find(name); task != _threads.end()) {
                // 작동중이면 true
                if (!task->second.get_is_done()) {
                    return true;
                }
                _threads.erase(task);
            }
            return false;
        }
    };

} // namespace ctls
