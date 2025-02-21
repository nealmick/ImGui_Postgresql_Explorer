cmake_minimum_required(VERSION 3.10)
project(DB_Explorer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# PostgreSQL
set(PostgreSQL_INCLUDE_DIRS "/usr/local/opt/libpq/include")
set(PostgreSQL_LIBRARY_DIRS "/usr/local/opt/libpq/lib")

# ImGui source files
set(IMGUI_SOURCES
    lib/imgui/imgui.cpp
    lib/imgui/imgui_demo.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/backends/imgui_impl_glfw.cpp
    lib/imgui/backends/imgui_impl_opengl3.cpp
)

# Main executable
add_executable(db_explorer
    main.cpp
    Table.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(db_explorer PRIVATE
    lib/imgui
    lib/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PostgreSQL_INCLUDE_DIRS}
)

target_link_libraries(db_explorer PRIVATE
    OpenGL::GL
    glfw
    ${PostgreSQL_LIBRARY_DIRS}/libpq.dylib
)

if(APPLE)
    target_link_libraries(db_explorer PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework OpenGL"
    )
endif()