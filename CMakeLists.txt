cmake_minimum_required(VERSION 3.10)
project(blender_clone)

# Hide ZERO_CHECK by disabling CMake auto-regeneration
set(CMAKE_SUPPRESS_REGENERATION TRUE)

# Group standard CMake targets (like ALL_BUILD) into a folder to keep the root Solution Explorer clean
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Gather source files
file(GLOB SOURCES 
    "src/*.cpp"
    "rlImGui/*.cpp"
    "imgui/*.cpp"
)

# Define the executable
add_executable(main ${SOURCES})

# Include directories common to all platforms
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

# Platform-specific configurations
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-linux")
    
    target_include_directories(main PRIVATE "${RAYLIB_DIR}/include")
    target_link_directories(main PRIVATE "${RAYLIB_DIR}/lib")
    
    # Linux specific libraries from the original Makefile
    target_link_libraries(main PRIVATE raylib GL m pthread dl rt X11)
    
    # Set RPATH so the executable can find the shared library
    set_target_properties(main PROPERTIES
        BUILD_RPATH "${RAYLIB_DIR}/lib"
        INSTALL_RPATH "${RAYLIB_DIR}/lib"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(MSVC)
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-msvc")
        target_include_directories(main PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(main PRIVATE "${RAYLIB_DIR}/lib")
        
        # MSVC specific libraries for Raylib
        target_link_libraries(main PRIVATE raylib.lib winmm.lib gdi32.lib shell32.lib user32.lib opengl32.lib)
    elseif(MINGW)
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-mingw")
        target_include_directories(main PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(main PRIVATE "${RAYLIB_DIR}/lib")
        
        # MinGW specific libraries for Raylib
        target_link_libraries(main PRIVATE raylib opengl32 gdi32 winmm pthread)
    else()
        # Fallback for Windows in case compiler isn't matched
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-mingw")
        target_include_directories(main PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(main PRIVATE "${RAYLIB_DIR}/lib")
        target_link_libraries(main PRIVATE raylib opengl32 gdi32 winmm pthread)
    endif()
endif()

# Set the output directory to ./bin
# set_target_properties(main PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
#     RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
#     RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
# )

# Set the default startup project for Visual Studio
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT main)
