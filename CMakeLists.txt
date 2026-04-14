cmake_minimum_required(VERSION 3.10)
project(btw)
set(CMAKE_SUPPRESS_REGENERATION TRUE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES
    "src/*.cpp"
    "src/core/*.cpp"
    "src/ui/*.cpp"
    "src/builtin_objects/*.cpp"
    "rlImGui/*.cpp"
    "imgui/*.cpp"
)

add_executable(btw ${SOURCES})

target_include_directories(btw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(btw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_include_directories(btw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rlImGui)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-linux")
    
    target_include_directories(btw PRIVATE "${RAYLIB_DIR}/include")
    target_link_directories(btw PRIVATE "${RAYLIB_DIR}/lib")
    
    target_link_libraries(btw PRIVATE raylib GL m pthread dl rt X11)
    
    set_target_properties(btw PROPERTIES
        BUILD_RPATH "${RAYLIB_DIR}/lib"
        INSTALL_RPATH "${RAYLIB_DIR}/lib"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(MSVC)
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-msvc")
        target_include_directories(btw PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(btw PRIVATE "${RAYLIB_DIR}/lib")

        # MSVC
        target_link_libraries(btw PRIVATE raylib.lib winmm.lib gdi32.lib shell32.lib user32.lib opengl32.lib)
    elseif(MINGW)
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-mingw")
        target_include_directories(btw PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(btw PRIVATE "${RAYLIB_DIR}/lib")
        
        # MinGW
        target_link_libraries(btw PRIVATE raylib opengl32 gdi32 winmm pthread)
    else()
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-mingw")
        target_include_directories(btw PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(btw PRIVATE "${RAYLIB_DIR}/lib")
        target_link_libraries(btw PRIVATE raylib opengl32 gdi32 winmm pthread)
    endif()
endif()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT btw)
