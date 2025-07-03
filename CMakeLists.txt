cmake_minimum_required(VERSION 3.10)
project(XafTool)

set(CMAKE_CXX_STANDARD 23)

include_directories (${CMAKE_SOURCE_DIR}/includes)

add_executable(${PROJECT_NAME} XafTool.cpp)