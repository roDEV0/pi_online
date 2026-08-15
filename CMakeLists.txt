cmake_minimum_required(VERSION 4.3)
project(pi_online)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBNM REQUIRED IMPORTED_TARGET libnm)

set(CMAKE_CXX_STANDARD 20)

add_executable(pi_online main.cpp)

target_link_libraries(pi_online PRIVATE PkgConfig::LIBNM)
