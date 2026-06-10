cmake_minimum_required(VERSION 3.20)

project(thesis)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    ${PROJECT_NAME}
    backtest/main.cpp
    src/core/market_events.cpp
    src/core/order_events.cpp
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)