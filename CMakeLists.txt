cmake_minimum_required(VERSION 3.20)

project(sfml-apps CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Use an installed SFML 3 via its CMake config package -- no download, no
# source build. Requesting version 3 rejects a system SFML 2.x (which would
# otherwise fail to compile the SFML 3 sources). SFML::Graphics pulls Window +
# System transitively and, unlike pkg-config, carries OpenGL/FreeType and the
# static-vs-shared link interface correctly.
#
# If CMake can't find it, point at your install:
#   -DSFML_DIR=<prefix>/lib/cmake/SFML   (or add <prefix> to CMAKE_PREFIX_PATH)
set(SFML_STATIC_LIBRARIES ON)
find_package(SFML 3 REQUIRED COMPONENTS Graphics)

# Each app lives under apps/<name>/ and is added here.
add_subdirectory(apps/maze)
