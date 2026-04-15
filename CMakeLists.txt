cmake_minimum_required(VERSION 3.10)
project(melder)
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

# Lua library (compiled as C)
file(GLOB LUA_SOURCES "lua-5.5.0/src/*.c")
list(REMOVE_ITEM LUA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.5.0/src/lua.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.5.0/src/luac.c"
)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.5.0/src")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(lua PRIVATE m)
endif()

add_executable(melder ${SOURCES})
target_link_libraries(melder PRIVATE lua)

target_include_directories(melder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(melder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_include_directories(melder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rlImGui)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-linux")

    target_include_directories(melder PRIVATE "${RAYLIB_DIR}/include")
    target_link_directories(melder PRIVATE "${RAYLIB_DIR}/lib")

    target_link_libraries(melder PRIVATE raylib GL m pthread dl rt X11)

    set_target_properties(melder PROPERTIES
        BUILD_RPATH "${RAYLIB_DIR}/lib"
        INSTALL_RPATH "${RAYLIB_DIR}/lib"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(MSVC)
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-msvc")
        target_include_directories(melder PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(melder PRIVATE "${RAYLIB_DIR}/lib")

        # MSVC
        target_link_libraries(melder PRIVATE
            raylib.lib
            winmm.lib
            gdi32.lib
            shell32.lib
            user32.lib
            opengl32.lib
        )
    elseif(MINGW)
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-mingw")
        target_include_directories(melder PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(melder PRIVATE "${RAYLIB_DIR}/lib")

        # MinGW
        target_link_libraries(melder PRIVATE raylib opengl32 gdi32 winmm pthread)
    else()
        set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib-mingw")
        target_include_directories(melder PRIVATE "${RAYLIB_DIR}/include")
        target_link_directories(melder PRIVATE "${RAYLIB_DIR}/lib")
        target_link_libraries(melder PRIVATE raylib opengl32 gdi32 winmm pthread)
    endif()
endif()

# copy example files to build/examples/
file(GLOB EXAMPLE_FILES "examples/*.scene" "examples/*.lua")
foreach(EXAMPLE ${EXAMPLE_FILES})
    get_filename_component(FNAME ${EXAMPLE} NAME)
    configure_file(${EXAMPLE} "${CMAKE_BINARY_DIR}/examples/${FNAME}" COPYONLY)
endforeach()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT melder)
