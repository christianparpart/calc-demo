cmake_minimum_required(VERSION 3.0)
project(calc-demo VERSION "0.0.0" LANGUAGES CXX)

add_executable(calc-demo calc-demo.cpp)
set_target_properties(calc-demo PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
