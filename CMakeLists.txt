cmake_minimum_required(VERSION 3.16)
project(cartridge32 VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── SDL2 ──────────────────────────────────────────────────────────────────────
find_package(SDL2 REQUIRED)

# ── OpenGL ────────────────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)

# ── SQLite3 ───────────────────────────────────────────────────────────────────
add_library(sqlite3 STATIC src/vendor/sqlite3/sqlite3.c)
target_include_directories(sqlite3 PUBLIC src/vendor/sqlite3)
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

# ── Dear ImGui ────────────────────────────────────────────────────────────────
set(IMGUI_DIR src/vendor/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)
if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
endif()

# ── cartridge32 ───────────────────────────────────────────────────────────────
set(C32_SOURCES
    src/main.cpp
    src/app.cpp
    src/db.cpp
    src/importer.cpp
    src/launcher.cpp
    src/scraper.cpp
    src/texture_cache.cpp
    src/screens/library.cpp
)

add_executable(cartridge32 ${C32_SOURCES})

target_include_directories(cartridge32 PRIVATE src)

# SDL2main must come BEFORE SDL2 in the list, then SDL2 again after
# so the linker can resolve SDL2main's references to SDL2
target_link_libraries(cartridge32 PRIVATE
    imgui
    sqlite3
    SDL2::SDL2main
    SDL2::SDL2
    OpenGL::GL
)

if(WIN32)
    target_link_libraries(cartridge32 PRIVATE winmm imm32 version setupapi shlwapi shell32 comdlg32 wininet windowscodecs ole32)
endif()

if(MSVC)
    target_compile_options(cartridge32 PRIVATE /W4 /WX-)
else()
    target_compile_options(cartridge32 PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Copy SDL2.dll on Windows
if(WIN32)
    get_target_property(_sdl2_dll SDL2::SDL2 IMPORTED_LOCATION)
    if(_sdl2_dll AND EXISTS "${_sdl2_dll}")
        add_custom_command(TARGET cartridge32 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_sdl2_dll}" "$<TARGET_FILE_DIR:cartridge32>"
            COMMENT "Copying SDL2.dll"
        )
    endif()
endif()
