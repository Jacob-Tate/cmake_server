# TODO: Actually write this
cmake_minimum_required(VERSION 3.12)
project(cmake_server C CXX)

if(CMAKE_COMPILER_IS_GNUXX OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(GCC 1)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    if(NOT CMAKE_ALLOW_APPLE_CLANG)
        message(FATAL_ERROR "Pretty sure appleclang doesnt work but define CMAKE_ALLOW_APPLE_CLANG to enable it")
    else()
        set(CLANG 1)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "[Cc]lang")
    set(CLANG 1)
elseif(MSVC)
    add_compile_options(/std:c++latest)
else()
    message(FATAL_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if(GCC OR CLANG)
    add_compile_options(-std=c++1z)
    if(WERROR)
        add_compile_options(-Wall -Wno-unknown-pragmas -Werror)
    endif()
endif()

file(GLOB_RECURSE CMAKE_SERVER_SOURCES src/cmake_server/*.cpp)

add_executable(driver src/driver.cpp ${CMAKE_SERVER_SOURCES})
target_include_directories(driver PRIVATE include)

if(CLANG)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("#include <iostream>
                               int main() { return __GLIBCXX__; }" USES_LIBSTDCXX)
    check_cxx_source_compiles("#include <iostream>
                               int main() { return _LIBCPP_VERSION; }" USES_LIBCXX)
    if ( NOT USES_LIBSTDCXX AND NOT USES_LIBCXX )
        message(FATAL_ERROR "Can't find which C++ runtime is in use")
    endif()
endif()

if(GCC OR (CLANG AND USES_LIBSTDCXX))
    target_link_libraries(vcpkg PRIVATE stdc++fs)
elseif(CLANG)
    target_link_libraries(vcpkg PRIVATE c++experimental)
endif()

if(WIN32)
    target_link_libraries(vcpkg PRIVATE bcrypt)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(driver PRIVATE Threads::Threads)
