cmake_minimum_required(VERSION 3.16)

# Retr01 UI: generic SDL widgets + Proggy Tiny. PNG chrome is injected by the host.

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

set(R01_UI_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_library(retr01_ui
  src/chrome.c
  src/draw/draw.c
  src/font/font.c
  src/widgets/button.c
  src/widgets/checkbox.c
  src/widgets/dot_strip.c
  src/widgets/modal.c
  src/widgets/multi_state.c
  src/widgets/panel.c
  src/widgets/radio.c
  src/widgets/slider.c
  src/widgets/tabs.c
  src/widgets/tab_pager.c
  src/widgets/textfield.c
)

target_include_directories(retr01_ui PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(retr01_ui PRIVATE
  "R01_UI_FONT_DIR=\"${R01_UI_ASSETS_DIR}\""
)
target_link_libraries(retr01_ui PUBLIC PkgConfig::SDL2 PkgConfig::FREETYPE)
