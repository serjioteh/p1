cmake_minimum_required(VERSION 3.0)

project(cmake_test)

add_subdirectory(gtest)

include_directories(gtest/include)

add_executable(${PROJECT_NAME} allocator.cpp allocator.h allocator_test.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_range_for)

target_link_libraries(${PROJECT_NAME} gtest gtest_main)
