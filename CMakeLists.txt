cmake_minimum_required(VERSION 3.20)

# Tell the CxxDroid IDE which built ELF to launch (it otherwise can't pick one
# among the project's outputs). Harmless comment on every other toolchain.
#Cxxdroid-binary: maze

project(sfml-apps CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# SFML is discovered in tiers because its version and packaging differ per
# environment: Debian ships SFML 3 (CMake config package, target SFML::Graphics),
# while Termux and CxxDroid ship SFML 2.x (non-namespaced targets sfml-graphics,
# or pkg-config only). Whichever is found is normalised onto the interface
# target `sfml` that the apps link against, so per-app CMake stays uniform.
#
# If CMake can't find it, point at your install:
#   -DSFML_DIR=<prefix>/lib/cmake/SFML   (or add <prefix> to CMAKE_PREFIX_PATH)
add_library(sfml INTERFACE)

# Prefer static SFML when its archive is on disk (e.g. CxxDroid, whose package
# is static-only). The probe only matches the static naming (libsfml-graphics-s),
# so a shared-only install leaves SFML_STATIC_LIBRARIES off.
find_library(SFML_GRAPHICS_STATIC
  NAMES sfml-graphics-s sfml-graphics-s-d
  PATH_SUFFIXES lib)
if(SFML_GRAPHICS_STATIC)
  message(STATUS "Static SFML found: ${SFML_GRAPHICS_STATIC}")
  set(SFML_STATIC_LIBRARIES ON)
else()
  set(SFML_STATIC_LIBRARIES OFF)
endif()

# Tier 1: SFML 3 config package (Debian). CONFIG bypasses CMake's legacy
# FindSFML.cmake module so the SFML 3 'Graphics' component resolves.
find_package(SFML 3 QUIET COMPONENTS Graphics CONFIG)
if(TARGET SFML::Graphics)
  message(STATUS "Using SFML 3 via CMake config package")
  target_link_libraries(sfml INTERFACE SFML::Graphics)
else()
  # Tier 2: SFML 2 finder. Components must be lowercase; the finder (CxxDroid's
  # FindSFML.cmake, or a 2.x config package) creates non-namespaced targets and
  # sfml-graphics pulls window+system and all native deps transitively.
  find_package(SFML 2 QUIET COMPONENTS graphics window system)
  if(TARGET sfml-graphics)
    message(STATUS "Using SFML 2 via CMake finder")
    target_link_libraries(sfml INTERFACE sfml-graphics sfml-window sfml-system)
  else()
    # Tier 3: pkg-config (Termux ships sfml-graphics.pc but no CMake package).
    message(STATUS "SFML CMake package not found; trying pkg-config")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SFML2 REQUIRED IMPORTED_TARGET sfml-graphics)
    target_link_libraries(sfml INTERFACE PkgConfig::SFML2)
  endif()
endif()

# Each app lives under apps/<name>/ and is added here.
add_subdirectory(apps/maze)
