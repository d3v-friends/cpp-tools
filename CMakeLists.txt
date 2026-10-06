cmake_minimum_required(VERSION 3.30)

project(cpp-tools C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find package
find_package(glm CONFIG REQUIRED)

# header only library
add_library(ctls INTERFACE)
add_library(ctls::ctls ALIAS ctls)

# include path
target_include_directories(ctls INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ctls INTERFACE glm::glm)

# 헤더파일 추가
file(GLOB_RECURSE CTLS_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
target_sources(
        ctls INTERFACE FILE_SET
        HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES ${CTLS_HEADERS}
)
target_compile_features(ctls INTERFACE cxx_std_20)


# test
option(CTLS_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
if (CTLS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif ()

