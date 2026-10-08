#include <gtest/gtest.h>
#include "ctls/ctls.hpp"

// test 이름은 PascalCase 로 한다.
// _ 언더바는 권장하지 않음
TEST(Crypto, UUID) {
    auto uuid = ctls::make_secure_uuid_v4();
    if (uuid.has_value()) {
        I_LOG_F("uuid={}", uuid.value());
        EXPECT_TRUE(true);
    } else {
        EXPECT_TRUE(false);
    }
}
