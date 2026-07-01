cmake_minimum_required(VERSION 3.20)

project(thesis)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    ${PROJECT_NAME}
    backtest_main.cpp
    src/core/market_events.cpp
    src/core/order_events.cpp
)

add_executable(
    optimizer
    optimize_main.cpp
    src/core/market_events.cpp
    src/core/order_events.cpp
)

add_executable(
    synthetic_compare
    synthetic_main.cpp
    src/core/market_events.cpp
    src/core/order_events.cpp
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(
    optimizer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(
    synthetic_compare
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-reorder-init-list)
    target_compile_options(optimizer PRIVATE -Wno-reorder-init-list)
    target_compile_options(synthetic_compare PRIVATE -Wno-reorder-init-list)
endif()
