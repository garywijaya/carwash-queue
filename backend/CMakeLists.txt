cmake_minimum_required(VERSION 3.14)
project(carwash_server)

set(CMAKE_CXX_STANDARD 17)

# Crow (header-only) — letakkan folder crow/ di root project
include_directories(${CMAKE_SOURCE_DIR}/crow/include)

find_package(Threads REQUIRED)

add_executable(server main.cpp)
target_link_libraries(server Threads::Threads)

# Jika pakai Asio standalone (tidak Boost):
target_compile_definitions(server PRIVATE ASIO_STANDALONE CROW_USE_BOOST=0)