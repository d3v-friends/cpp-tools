find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp") # 파일 목록을 불러온다
add_executable(ctls_test ${TEST_SOURCES})
target_link_libraries(
        ctls_test
        PRIVATE
        ctls::ctls
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
)

# clion 편의기능
include(GoogleTest)
gtest_discover_tests(ctls_test)