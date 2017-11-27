cmake_minimum_required(VERSION 3.1)
project(MoeUV)

# 第三方模块
if(NOT TARGET MoeCore)
    add_subdirectory(3rd/Moe.Core)
endif()

if(NOT TARGET uv)
    add_subdirectory(3rd/libuv)
endif()

# 编译选项
set(CMAKE_CXX_STANDARD 11)

if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0600 -D_GNU_SOURCE -D_CRT_SECURE_NO_WARNINGS)
    
    enable_language(CXX ASM_MASM)
else()
    set(CMAKE_CXX_FLAGS "-g -Wall -Wextra")
    set(CMAKE_C_FLAGS "-g -Wall -Wextra")
    
    enable_language(CXX ASM)
endif()

# 目标
file(GLOB_RECURSE MOE_UV_SRC src/*.cpp include/*.hpp)

# 根据平台决定Context汇编代码
if(UNIX)
    if(ANDROID) # Android
        if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
            set(MOE_UV_CPU_ARCH "arm")
            set(MOE_UV_CONTEXT_ASM_EXT "aapcs_elf_gas.S")
        elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
            set(MOE_UV_CPU_ARCH "arm64")
            set(MOE_UV_CONTEXT_ASM_EXT "aapcs_elf_gas.S")
        elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
            set(MOE_UV_CPU_ARCH "i386")
            set(MOE_UV_CONTEXT_ASM_EXT "sysv_elf_gas.S")
        elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
            set(MOE_UV_CPU_ARCH "x86_64")
            set(MOE_UV_CONTEXT_ASM_EXT "sysv_elf_gas.S")
        endif()
    else() # PC (x86/x64)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(MOE_UV_CPU_ARCH "x86_64")
        else()
            set(MOE_UV_CPU_ARCH "i386")
        endif()
        if(APPLE)
            set(MOE_UV_CONTEXT_ASM_EXT "sysv_macho_gas.S") # OSX
        else()
            set(MOE_UV_CONTEXT_ASM_EXT "sysv_elf_gas.S")  # Linux/Unix
        endif()
    endif()
elseif(WIN32) # Windows PC
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MOE_UV_CPU_ARCH "x86_64")
    else()
        set(MOE_UV_CPU_ARCH "i386")
    endif()
    if(MSVC)
        set(MOE_UV_CONTEXT_ASM_EXT "ms_pe_masm.asm")
    else()
        set(MOE_UV_CONTEXT_ASM_EXT "ms_pe_gas.S")
    endif()
endif()

set(MOE_UV_ASM_SOURCES
    "src/asm/make_${MOE_UV_CPU_ARCH}_${MOE_UV_CONTEXT_ASM_EXT}"
    "src/asm/jump_${MOE_UV_CPU_ARCH}_${MOE_UV_CONTEXT_ASM_EXT}")

add_library(MoeUV STATIC ${MOE_UV_SRC} ${MOE_UV_ASM_SOURCES})
target_link_libraries(MoeUV MoeCore uv)
target_include_directories(MoeUV PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
