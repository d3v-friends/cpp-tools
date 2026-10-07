# cmake 파일

~~~cmake
# ctls 라이브러리 불러오기
include(FetchContent)
FetchContent_Declare(
        ctls
        GIT_REPOSITORY https://github.com/d3v-friends/cpp-tools.git
        GIT_TAG v0.0.2
)
FetchContent_MakeAvailable(ctls)

target_link_libraries(PROGRAM PRIVATE ctls::ctls)
~~~