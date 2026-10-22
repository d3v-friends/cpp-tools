#include <gtest/gtest.h>
#include "ctls/ctls.hpp"

// test 이름은 PascalCase 로 한다.
// _ 언더바는 권장하지 않음
TEST(Logger, PrintLog) {
    ILOG_TRACE("TRACE");
    ILOG_DEBUG("DEBUG");
    ILOG_INFO("INFO");
    ILOG_WARN("WARN");
    ILOG_ERROR("ERROR");
    EXPECT_TRUE(true);
}
