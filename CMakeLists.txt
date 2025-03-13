cmake_minimum_required(VERSION 3.13)

project(clara VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(TARGET ${PROJECT_NAME})

#set weither clara is the main project or a subproject
set(MAIN_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MAIN_PROJECT ON)
endif()

#append cmake/ to the CMAKE_MODULE_PATH var
#set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ ${CMAKE_MODULE_PATH})

#TODO:
#include()

#build tests and export compile commands if main project
if(${MAIN_PROJECT})
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(DO_CLARA_BUILD_TESTS ON)#TODO on
else()
  set(DO_CLARA_BUILD_TESTS OFF)
endif()

option(CLARA_BUILD_TESTS "Build clara unit tests." ${DO_CLARA_BUILD_TESTS})
option(CLARA_USE_LIBFMT "clara uses fmt for string formatting and console output." OFF)

if(CLARA_USE_LIBFMT)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/fmt/)
endif()

add_library(${TARGET} INTERFACE)
add_library(${PROJECT_NAME}::${TARGET} ALIAS ${TARGET})


target_compile_definitions(${TARGET} INTERFACE
  $<$<BOOL:${CLARA_USE_LIBFMT}>:CLARA_USE_LIBFMT=1>
)

target_include_directories(${TARGET} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/)

if(CLARA_BUILD_TESTS)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests/)
endif()
