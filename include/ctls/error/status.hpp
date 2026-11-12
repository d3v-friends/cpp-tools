#pragma once
#include <array>
#include <chrono>
#include <concepts>
#include <memory>
#include <source_location>
#include <string>
#include <utility>

#include "failed_result.hpp"

namespace ctls {

    // 리턴값에 에러메세지를 포함해서 체크할수있게 도와준다.
    class [[nodiscard]] status {
    public:
        // 자동형변환 가능하게 만들기, 성공이면 {} 리턴가능
        status() noexcept = default;

        // NOLINTNEXTLINE
        status(failed_result f)
            : _location(f.location), _timestamp(f.timestamp),
              _message{std::move(f.message)}, _is_ok{false} {}

        status(const status&) = delete;

        status& operator=(const status&) = delete;

        status(status&&) = default;

        status& operator=(status&&) = default;

        // 생성자 감추고 간결하게 함수화
        static status ok() noexcept {
            return status{};
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return _is_ok;
        }
        [[nodiscard]] bool is_ok() const noexcept {
            return _is_ok;
        }
        [[nodiscard]] const std::string& get_message() const noexcept {
            return _message;
        }

        [[nodiscard]] const std::source_location& get_location() const noexcept {
            return _location;
        }

        [[nodiscard]] std::chrono::system_clock::time_point
        get_timestamp() const noexcept {
            return _timestamp;
        }

        // 실패 정보를 그대로 꺼내서 다른 result/status 로 전파
        [[nodiscard]] failed_result to_failed() const& {
            // const 객체: 메세지 복사
            return failed_result{
                .message = _message,
                .location = _location,
                .timestamp = _timestamp,
            };
        }

        [[nodiscard]] failed_result to_failed() && {
            // std::move(r): 메세지 이동
            return failed_result{
                .message = std::move(_message),
                .location = _location,
                .timestamp = _timestamp,
            };
        }

        // 체인 맨 끝에 before 를 붙인다 (이미 체인이 있으면 그 뒤에 붙음)
        void append_chain(status before) {
            auto tail = this;
            while (tail->_chain) {
                tail = tail->_chain.get();
            }
            tail->_chain = std::make_unique<status>(std::move(before));
        }

        [[nodiscard]] const status* get_chain() const noexcept {
            return _chain.get();
        }

    protected:
        std::source_location _location;
        std::chrono::system_clock::time_point _timestamp;
        std::unique_ptr<status> _chain = nullptr; // 에러체인
        std::string _message;
        bool _is_ok = true;
    };

    inline status ok() noexcept {
        return status{};
    }

} // namespace ctls