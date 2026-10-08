#include <gtest/gtest.h>
#include "ctls/ctls.hpp"

// test 이름은 PascalCase 로 한다.
// _ 언더바는 권장하지 않음
TEST(String, to_string) { EXPECT_TRUE(ctls::to_string(123) == "123"); }
TEST(String, to_int) {
    EXPECT_TRUE(ctls::to_int("123") == 123);
    EXPECT_TRUE(ctls::to_string(123.5) == "123.5");
}