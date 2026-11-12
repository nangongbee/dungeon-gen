cmake_minimum_required(VERSION 3.16)
project(dungeon_gen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(dungeon_core
  src/rng.cpp
  src/config.cpp
  src/generate.cpp
  src/query.cpp
  src/session.cpp
  src/ascii.cpp
)
target_include_directories(dungeon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(dungeon_cli apps/cli/main.cpp)
target_link_libraries(dungeon_cli PRIVATE dungeon_core)

add_executable(dungeon_tests tests/test_invariants.cpp)
target_link_libraries(dungeon_tests PRIVATE dungeon_core)

option(DUNGEON_BUILD_PLAY "Build the SDL2 + ImGui viewer" ON)

find_package(SDL2 QUIET)
if(DUNGEON_BUILD_PLAY AND SDL2_FOUND)
  # ImGui 没有自带 CMakeLists.txt；FetchContent 只负责下载，target 在下面自己定义。
  # 可用 -DFETCHCONTENT_BASE_DIR=... 把下载缓存放到构建目录外（CI 会缓存 .fc-cache）。
  include(FetchContent)
  FetchContent_Declare(imgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(imgui_src)

  add_library(imgui
    ${imgui_src_SOURCE_DIR}/imgui.cpp
    ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
  )
  target_include_directories(imgui PUBLIC
    ${imgui_src_SOURCE_DIR}
    ${imgui_src_SOURCE_DIR}/backends
  )
  # 上游代码不参与本项目的警告检查
  target_compile_options(imgui PRIVATE -w)
  if(TARGET SDL2::SDL2)
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
    set(DUNGEON_SDL_LIBS SDL2::SDL2)
  else()
    target_include_directories(imgui PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(imgui PUBLIC ${SDL2_LIBRARIES})
    set(DUNGEON_SDL_LIBS ${SDL2_LIBRARIES})
  endif()

  add_executable(dungeon_play apps/play/main.cpp)
  target_link_libraries(dungeon_play PRIVATE dungeon_core imgui ${DUNGEON_SDL_LIBS})
elseif(NOT DUNGEON_BUILD_PLAY)
  message(STATUS "DUNGEON_BUILD_PLAY=OFF; skipping dungeon_play")
else()
  message(STATUS "SDL2 not found; skipping dungeon_play (core and tests still build)")
endif()

enable_testing()
add_test(NAME invariants COMMAND dungeon_tests)
