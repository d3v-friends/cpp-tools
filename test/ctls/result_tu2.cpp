// [2026-11-13] 두 번째 cpp: ctls::ok(inline constexpr) / result<void> 를 다른 TU 에서도 써서 중복 정의 링크 에러가 없는지 확인
#include <ctls/error/result.hpp>

#include <string>

ctls::result<void> result_tu2_ok() {
    return ctls::ok;
}
ctls::result<void> result_tu2_failed() {
    return ctls::failed(std::string{"tu2"});
}
