cmake_minimum_required(VERSION 3.10)
project(RPGInventory LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(RPGInventory src/main.cpp)

target_include_directories(RPGInventory PRIVATE src)
