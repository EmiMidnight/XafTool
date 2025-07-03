cmake_minimum_required(VERSION 3.15)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
project(XafTool)

set(CMAKE_CXX_STANDARD 23)

include_directories (${CMAKE_SOURCE_DIR}/includes)

add_executable(${PROJECT_NAME} XafTool.cpp)