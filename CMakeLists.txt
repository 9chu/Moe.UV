cmake_minimum_required(VERSION 3.1)
project(MoeUV)

# 第三方模块
if(NOT TARGET MoeCore)
    add_subdirectory(3rd/Moe.Core)
endif()

if(NOT TARGET uv_a)
    add_subdirectory(3rd/libuv)
endif()

# 编译选项
set(CMAKE_CXX_STANDARD 11)
if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0600 -D_GNU_SOURCE -D_CRT_SECURE_NO_WARNINGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -Wno-implicit-fallthrough")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -Wno-implicit-fallthrough")
endif()

# 目标
file(GLOB_RECURSE MOE_UV_SRC src/*.cpp include/*.hpp)

add_library(MoeUV STATIC ${MOE_UV_SRC})
target_link_libraries(MoeUV MoeCore MoeCoroutine uv_a)
target_include_directories(MoeUV PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
