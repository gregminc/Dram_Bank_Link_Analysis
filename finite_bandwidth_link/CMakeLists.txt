
cmake_minimum_required(VERSION 3.20)

project(bank_link_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bank_link_core
    src/address_mapper.cpp
    src/bank_link_mapper.cpp
    src/link.cpp
    src/request_builder.cpp
    src/request_fifo.cpp
    src/simulator.cpp
    src/statistics.cpp
    src/trace_reader.cpp
)

target_include_directories(bank_link_core PUBLIC include)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(bank_link_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

add_executable(bank_link_sim src/main.cpp)
target_link_libraries(bank_link_sim PRIVATE bank_link_core)

enable_testing()

add_executable(request_fifo_test tests/request_fifo_test.cpp)
target_link_libraries(request_fifo_test PRIVATE bank_link_core)
add_test(NAME request_fifo_test COMMAND request_fifo_test)

add_executable(link_test tests/link_test.cpp)
target_link_libraries(link_test PRIVATE bank_link_core)
add_test(NAME link_test COMMAND link_test)

add_test(
    NAME simulator_smoke_test
    COMMAND bank_link_sim
        --trace ${CMAKE_CURRENT_SOURCE_DIR}/tests/sample.trace
        --banks-per-link 2
        --link-width-bits 16
        --transfers-per-ns 4.266
        --fifo-depth 4
)
