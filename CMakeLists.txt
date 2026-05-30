cmake_minimum_required(VERSION 3.20)
project(OrderBook VERSION 1.0 LANGUAGES CXX)

add_compile_options(-g -Wall -Wextra -pedantic-errors -Werror -O3 -march=native)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(SyscallExchange)
target_include_directories(SyscallExchange PRIVATE ./)
target_sources(SyscallExchange
  PUBLIC
    SPSC/main.cpp SPSC/Producer.cpp OrderBook/OrderBook.cpp
)
target_link_libraries(SyscallExchange PRIVATE uring)

add_executable(UringExchange)
target_include_directories(UringExchange PRIVATE ./)
target_sources(UringExchange
  PUBLIC
    io_uring/main.cpp OrderBook/OrderBook.cpp
)
target_link_libraries(UringExchange PRIVATE uring)