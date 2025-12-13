cmake_minimum_required(VERSION 3.7)

set(PROJECT_NAME RI)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable all compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

message(STATUS "Configuring project ${PROJECT_NAME} (${PROJECT_VERSION})")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(RI
    src/RI.cpp
)

add_subdirectory(deps/minifb)

target_link_libraries(${PROJECT_NAME} PRIVATE minifb)
