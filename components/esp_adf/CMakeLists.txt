cmake_minimum_required(VERSION 3.0)

project(CopyMediaPlayer)

# Define source and destination directories
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/media_player)
set(DEST_DIR ${CMAKE_BINARY_DIR}/media_player)

# Custom command to copy media_player directory
add_custom_command(
    OUTPUT ${DEST_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_DIR} ${DEST_DIR}
    COMMENT "Copying media_player directory to build folder"
)

# Custom target that depends on the copy command
add_custom_target(copy_media_player ALL DEPENDS ${DEST_DIR})

# Add your other targets here
# add_executable(my_executable source_file.cpp)

# Ensure the custom target is built before any other target
add_dependencies(your_main_target copy_media_player)
