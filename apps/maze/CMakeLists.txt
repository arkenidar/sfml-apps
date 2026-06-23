add_executable(maze main.cpp)

target_compile_features(maze PRIVATE cxx_std_17)

# SFML::Graphics pulls in Window and System transitively.
target_link_libraries(maze PRIVATE SFML::Graphics)

# Copy the shared assets next to the executable so the app finds them
# regardless of the directory it is launched from.
add_custom_command(TARGET maze POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:maze>/assets
  COMMENT "Copying assets next to the maze executable")
