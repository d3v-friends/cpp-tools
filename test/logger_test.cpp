#include <gtest/gtest.h>
#include "ctls/ctls.hpp"

// test 이름은 PascalCase 로 한다.
// _ 언더바는 권장하지 않음
TEST(Logger, PrintLog) {
    I_LOG("hello");
    EXPECT_TRUE(true);
}
