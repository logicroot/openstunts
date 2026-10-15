cmake_minimum_required(VERSION 3.20)
project(stunts3d LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(STUNTS3D_WERROR "Treat warnings as errors" OFF)

# ---- Dependencies -----------------------------------------------------------
find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# GLM (header only, vendored in third_party/glm)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm)

# ---- Core library -----------------------------------------------------------
add_library(stunts3d_core STATIC
  src/render/gl_loader.cpp
  src/render/shader.cpp
  src/render/mesh.cpp
  src/render/renderer.cpp
  src/render/screenshot.cpp
  src/render/texture.cpp
  src/render/hud.cpp
  src/render/sprite_renderer.cpp
  src/platform/window.cpp
  src/platform/input.cpp
  src/assets/binary_reader.cpp
  src/assets/trk.cpp
  src/assets/res_file.cpp
  src/assets/car.cpp
  src/assets/compression.cpp
  src/assets/shape3d.cpp
  src/assets/palette.cpp
  src/assets/font.cpp
  src/assets/sprite2d.cpp
  src/assets/crash_overlay.cpp
  src/assets/opponent.cpp
  src/physics/fixed_math.cpp
  src/physics/car_engine.cpp
  src/physics/car_steering.cpp
  src/physics/car_motion.cpp
  src/physics/car_collision.cpp
  src/physics/car_debris.cpp
  src/physics/track_collision.cpp
  src/physics/opponent_ai.cpp
  src/world/track_elevation.cpp
  src/world/track_mesh.cpp
  src/world/car_mesh.cpp
  src/world/skybox.cpp
  src/world/dashboard.cpp
  src/world/shape_mesh.cpp
  src/world/track_objects.cpp
  src/world/track_shapes.cpp
  src/world/track_route.cpp
  src/world/material.cpp
  src/core/paths.cpp
  src/core/config.cpp
  src/ui/menu.cpp
  src/ui/gui.cpp
  src/game/application.cpp
  src/game/debug_track_scene.cpp
  src/game/drive_scene.cpp
  src/game/frontend_scene.cpp
  src/game/race_state.cpp
)
target_include_directories(stunts3d_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)
target_link_libraries(stunts3d_core PUBLIC glm SDL2::SDL2 OpenGL::GL)

if(WIN32)
  target_link_libraries(stunts3d_core PUBLIC opengl32)
endif()

if(MSVC)
  target_compile_options(stunts3d_core PRIVATE /W4)
else()
  target_compile_options(stunts3d_core PRIVATE -Wall -Wextra)
endif()

# ---- Game executable --------------------------------------------------------
add_executable(stunts3d src/main.cpp)
target_link_libraries(stunts3d PRIVATE stunts3d_core)
if(TARGET SDL2::SDL2main)
  target_link_libraries(stunts3d PRIVATE SDL2::SDL2main)
endif()

# ---- Tools ------------------------------------------------------------------
add_executable(s3d_dump tools/dump/main.cpp)
target_link_libraries(s3d_dump PRIVATE stunts3d_core)

# ---- Tests ------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
