# ----------------------------------------------------------------------------
#  govno_core — ядро симулятора (модель цепи, расчёт, сохранение).
#
#  Сейчас это INTERFACE-библиотека с одним заголовком (контракт типов
#  элементов). Когда появятся .cpp файлы ядра, замените на:
#      add_library(govno_core STATIC src/Circuit.cpp ...)
#      target_include_directories(govno_core PUBLIC include)
#  gui при этом менять не нужно — он линкуется с govno::core.
#
#  ПРАВИЛО: ядро не зависит от Qt Widgets/Gui. Допустим максимум QtCore.
# ----------------------------------------------------------------------------
add_library(govno_core INTERFACE)
add_library(govno::core ALIAS govno_core)
target_include_directories(govno_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(govno_core INTERFACE cxx_std_17)

# Проверка расчётов «Печатных плат» по контрольным значениям (без Qt): cmake -DGOVNO_BUILD_TESTS=ON, затем ctest.
if(GOVNO_BUILD_TESTS)
    add_executable(pcb_check tests/pcb_check.cpp)
    target_link_libraries(pcb_check PRIVATE govno::core)
    add_test(NAME pcb_check COMMAND pcb_check)
endif()
