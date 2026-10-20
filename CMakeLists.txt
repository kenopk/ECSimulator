cmake_minimum_required(VERSION 3.16)

project(GOVNO VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Две независимые части проекта:
#   core — ядро симулятора (без GUI)
#   gui  — визуальная оболочка (Qt Widgets), использует только публичный API ядра
# Тесты ядра (без Qt) включаются ключом -DGOVNO_BUILD_TESTS=ON; запуск — ctest.
option(GOVNO_BUILD_TESTS "Собрать тесты ядра" OFF)
if(GOVNO_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)
add_subdirectory(gui)
