cmake_minimum_required(VERSION 3.1...3.14)
project(clara_tests)

add_subdirectory(vendor/googletest/)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(TARGET ${PROJECT_NAME})

add_executable(${TARGET} src/main.test.cpp)
target_link_libraries(${TARGET} clara::clara gtest_main)
target_include_directories(${TARGET} PRIVATE ../include/)
