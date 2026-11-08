#pragma once
#include <string>
#include <utility>
#include <source_location>
#include <chrono>

#include "failed_result.hpp"

namespace ctls {

    // 리턴값에 에러메세지를 포함해서 체크할수있게 도와준다.
    class [[nodiscard]] status {
    public:
        // 자동형변환 가능하게 만들기, 성공이면 {} 리턴가능
        status() noexcept = default;


        //NOLINTNEXTLINE
        status(failed_result f) : _location(f.location),
                                  _timestamp(f.timestamp),
                                  _message{std::move(f.message)},
                                  _is_ok{false} {
        }


        status(const status&) = delete;

        status& operator=(const status&) = delete;

        status(status&&) = default;

        status& operator=(status&&) = default;

        // 생성자 감추고 간결하게 함수화
        static status ok() noexcept { return status{}; }

        [[nodiscard]] explicit operator bool() const noexcept { return _is_ok; }
        [[nodiscard]] bool is_ok() const noexcept { return _is_ok; }
        [[nodiscard]] const std::string& get_message() const noexcept { return _message; }

        [[nodiscard]] const std::source_location& get_location() const noexcept {
            return _location;
        }

        [[nodiscard]] std::chrono::system_clock::time_point
        get_timestamp() const noexcept {
            return _timestamp;
        }

    protected:
        std::source_location _location;
        std::chrono::system_clock::time_point _timestamp;
        std::string _message;
        bool _is_ok = true;
    };

} // namespace ctls