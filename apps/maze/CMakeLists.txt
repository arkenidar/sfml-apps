add_executable(maze main.cpp)

target_compile_features(maze PRIVATE cxx_std_17)

# `sfml` is the interface target defined in the top-level CMakeLists; it maps to
# SFML 2 or 3 depending on what the build environment provides.
target_link_libraries(maze PRIVATE sfml)

# Copy the shared assets next to the executable so the app finds them
# regardless of the directory it is launched from.
add_custom_command(TARGET maze POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:maze>/assets
  COMMENT "Copying assets next to the maze executable")
