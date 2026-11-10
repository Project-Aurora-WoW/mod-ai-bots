# mod-ai-bots — AI-боты для AzerothCore
#
# Сборка как часть AzerothCore modules/. Используются только стандартные
# команды CMake: ядро собирает исходники модуля самостоятельно
# (modules/CMakeLists.txt -> CollectSourceFiles), поэтому файл нужен
# для совместимости со сборками, которые подключают модуль через add_subdirectory.

file(GLOB_RECURSE AI_BOTS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_library(mod-ai-bots STATIC
  ${AI_BOTS_SOURCES})

target_include_directories(mod-ai-bots
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(mod-ai-bots
  PRIVATE
    game
    scripts)

set_target_properties(mod-ai-bots PROPERTIES FOLDER "modules")
