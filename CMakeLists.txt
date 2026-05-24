cmake_minimum_required(VERSION 3.20)
project(Exchange VERSION 1.0 LANGUAGES CXX)

add_compile_options(-g -Wall -Wextra -pedantic-errors -Werror -O3)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Exchange)

target_include_directories(Exchange PRIVATE ./)

target_sources(Exchange
  PUBLIC
    main.cpp SPSC/Producer.cpp OrderBook/OrderBook.cpp
)