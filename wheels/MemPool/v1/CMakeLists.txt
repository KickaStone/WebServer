cmake_minimum_required(VERSION 3.10)

project(mempool_v1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} UnitTest.cpp MemoryPool_v1.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE)
target_link_libraries(${PROJECT_NAME} pthread)