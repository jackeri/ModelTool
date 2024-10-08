cmake_minimum_required(VERSION 3.20)
include(CheckIncludeFile)
include(ExternalProject)

project(ModelTool VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_TARGETS_FOLDER CMake)
set(BUNDLED_TARGETS_FOLDER Bundled)

option(test "Enable testing" ON)
option(opengl3 "Enable opengl3 core backend" OFF)
option(NATIVE_DIALOG "Use platform native dialog" OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ${CMAKE_TARGETS_FOLDER})

# Activate C++ 17 and C99 (if needed)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)

IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
    MESSAGE("No CMAKE_BUILD_TYPE specified, defaulting to ${CMAKE_BUILD_TYPE}")
ENDIF (NOT CMAKE_BUILD_TYPE)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(DEBUG_MODE 1)
else ()
    set(DEBUG_MODE 0)
endif ()

# Setup static runtime linking in Windows (Msvc and Mingw)
if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /EHsc /O2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /EHa /W3")
    set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            )

    foreach (CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach ()

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT.lib /NODEFAULTLIB:MSVCRTD.lib")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D_DEBUG")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG")

    add_definitions(-D_CRT_SECURE_NO_WARNINGS) # Do not show CRT warnings
    add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)

    if (MSVC_IDE)
        if (MSVC_VERSION EQUAL 1700)
            set(CMAKE_GENERATOR_TOOLSET "v120_xp" CACHE STRING "Platform Toolset" FORCE)
        elseif (MSVC_VERSION EQUAL 1800)
            set(CMAKE_GENERATOR_TOOLSET "v120_xp" CACHE STRING "Platform Toolset" FORCE)
        elseif (MSVC_VERSION EQUAL 1910)
            set(CMAKE_GENERATOR_TOOLSET "v141_xp" CACHE STRING "Platform Toolset" FORCE)
        endif ()
    endif (MSVC_IDE)
elseif (MINGW AND CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -static-libgcc -static")
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static-libgcc -static-libstdc++ -static")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -s -static")
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS} -static-libgcc -liconv -s -static")
    set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} -static-libgcc -static-libstdc++ -liconv -s -static")
    add_definitions(-D_WIN32_IE=0x0501)
endif ()

# some linux systems provide a wrapper (GLVND) for opengl, but lets stick to the legacy version for now
SET(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

FILE(GLOB MODEL_TOOL_APP_SRC "src/app/*.*")
FILE(GLOB MODEL_TOOL_LIB_SRC "src/lib/*.*")
FILE(GLOB MODEL_TOOL_CLI_SRC "src/cli/*.*")

list(APPEND MODEL_TOOL_APP_SRC ${MODEL_TOOL_SHADERS})
source_group("Shaders\\" FILES ${MODEL_TOOL_SHADERS})

set(ZLIB_LIB "libs/zlib")
FILE(GLOB ZLIB_LIB_HEADERS
        ${ZLIB_LIB}/crc32.h
        ${ZLIB_LIB}/gzguts.h
        ${ZLIB_LIB}/inffast.h
        ${ZLIB_LIB}/inffixed.h
        ${ZLIB_LIB}/inflate.h
        ${ZLIB_LIB}/inftrees.h
        ${ZLIB_LIB}/zconf.h
        ${ZLIB_LIB}/zlib.h
        ${ZLIB_LIB}/zutil.h
        )

FILE(GLOB ZLIB_LIB_SRC
        ${ZLIB_LIB}/adler32.c
        ${ZLIB_LIB}/crc32.c
        ${ZLIB_LIB}/inffast.c
        ${ZLIB_LIB}/inflate.c
        ${ZLIB_LIB}/inftrees.c
        ${ZLIB_LIB}/zutil.c
        )

include_directories(${ZLIB_LIB})
add_library(bundled_zlib STATIC ${ZLIB_LIB_SRC} ${ZLIB_LIB_HEADERS})

set(MINIZIP_LIB "libs/minizip")

FILE(GLOB MINIZIP_LIB_HEADERS
        ${MINIZIP_LIB}/crypt.h
        ${MINIZIP_LIB}/ioapi.h
        ${MINIZIP_LIB}/unzip.h
        )

FILE(GLOB MINIZIP_LIB_SRC
        ${MINIZIP_LIB}/ioapi.c
        ${MINIZIP_LIB}/unzip.c
        )
include_directories(${MINIZIP_LIB})
add_library(bundled_minizip STATIC ${MINIZIP_LIB_SRC} ${MINIZIP_LIB_HEADERS})
target_link_libraries(bundled_minizip bundled_zlib)

# nativefiledialog
if (NATIVE_DIALOG)
    set(NFD_LIB "libs/nativefiledialog")
    FILE(GLOB NFD_SRC
            ${NFD_LIB}/src/common.h
            ${NFD_LIB}/src/nfd_common.c
            ${NFD_LIB}/src/nfd_common.h
            )

    if (WIN32)
        LIST(APPEND NFD_SRC "${NFD_LIB}/src/nfd_win.cpp")
    elseif (APPLE)
        LIST(APPEND NFD_SRC "${NFD_LIB}/src/nfd_cocoa.m")
    else ()
        LIST(APPEND NFD_SRC "${NFD_LIB}/src/nfd_gtk.c")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
        include_directories(${GTK3_INCLUDE_DIRS})
        link_directories(${GTK3_LIBRARY_DIRS})
        # Add other flags to the compiler
        add_definitions(${GTK3_CFLAGS_OTHER})
    endif ()

	include_directories(${NFD_LIB}/src/include)
	add_library(bundled_nfd ${NFD_SRC})

	IF (UNIX AND NOT APPLE)
		target_link_libraries(bundled_nfd ${GTK3_LIBRARIES})
	ENDIF ()

    set_target_properties(bundled_nfd PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
else ()
	# Dummy target for nfd
	# add_custom_target(bundled_nfd)
	add_library(bundled_nfd INTERFACE)
endif ()

FILE(GLOB IMGUI_SRC
        "libs/imgui/imgui.cpp"
        "libs/imgui/imgui.h"
        "libs/imgui/imgui_draw.cpp"
        "libs/imgui/imgui_internal.h"
        "libs/imgui/imgui_tables.cpp"
        "libs/imgui/imgui_widgets.cpp"
        "libs/imgui/imconfig.h"
        "libs/imgui/imstb_rectpack.h"
        "libs/imgui/imstb_textedit.h"
        "libs/imgui/imstb_truetype.h"
        "libs/imgui/misc/cpp/imgui_stdlib.cpp"
        # Backends
        "libs/imgui/backends/imgui_impl_glfw.cpp"
        "libs/imgui/backends/imgui_impl_opengl3.cpp"
        "libs/imgui/backends/imgui_impl_opengl2.cpp"
        )

# Libs that come in the form of headers only (stb, tinydir etc)
FILE(GLOB LIB_HEADERS
        "libs/tinydir/tinydir.h"
        "libs/stb/stb_image.h"
        "libs/stb/stb_image_resize.h"
        "libs/stb/stb_image_write.h"
        )

include_directories("libs/imgui")
include_directories("libs/imgui-filebrowser")
include_directories("libs/stb")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/glfw)

add_subdirectory(libs/spdlog)

include(libs/glew-2.1.0/CMakeLists.txt)

# Add this when we need it
#add_subdirectory(libs/glm)

include_directories(libs/glew-2.1.0/include)
include_directories(libs/glm)


add_library(ModelToolLib STATIC ${MODEL_TOOL_LIB_SRC})
target_include_directories(ModelToolLib PUBLIC "src/lib")

add_executable(ModelToolCLI ${MODEL_TOOL_CLI_SRC})
set_target_properties(ModelToolCLI PROPERTIES OUTPUT_NAME "mt")
target_link_libraries(ModelToolCLI ModelToolLib)

set(MTLINK_LIBRARIES ModelToolLib glew ${OPENGL_LIBRARIES} glfw bundled_minizip bundled_zlib bundled_nfd spdlog)
add_executable(ModelTool WIN32 MACOSX_BUNDLE "src/main.cpp" ${MODEL_TOOL_APP_SRC} ${IMGUI_SRC} ${LIB_HEADERS})
add_dependencies(ModelTool glew glfw spdlog)
target_link_libraries(ModelTool ${MTLINK_LIBRARIES})

set_target_properties(glew PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(glfw PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(spdlog PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(bundled_minizip PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(bundled_zlib PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})

target_compile_definitions(ModelTool PUBLIC GLEW_STATIC)

if (opengl3)
    add_definitions(-DMT_OPENGL_3)
else ()
    add_definitions(-DMT_OPENGL_2)
endif ()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ModelTool)

set_target_properties(ModelTool PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ""
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ""
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ""
        )

install(TARGETS ModelTool DESTINATION "./")
install(DIRECTORY "fonts" DESTINATION "./")
install(DIRECTORY "shaders" DESTINATION "./")

file(COPY ${CMAKE_CURRENT_LIST_DIR}/fonts DESTINATION ${CMAKE_BINARY_DIR})

if (test)

    enable_testing()
    include(GoogleTest)

    # Try to use the system gtest package is exists
    find_package(GTest)

    if (NOT GTest_FOUND)
        add_subdirectory(libs/googletest)
    endif ()

    file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false
            "tst/*.h"
            "tst/*.cpp"
            )

    set(ALL_SOURCES ${TEST_SOURCES} ${MODEL_TOOL_APP_SRC} ${IMGUI_SRC} ${LIB_HEADERS})

    add_executable(mt_test ${ALL_SOURCES})
    add_dependencies(mt_test ModelTool)
    target_include_directories(mt_test PRIVATE "src/app")
    target_link_libraries(mt_test PUBLIC ${MTLINK_LIBRARIES})

    if (GTest_FOUND)
        target_link_libraries(mt_test PUBLIC GTest::gtest GTest::gtest_main)
    else ()
        target_link_libraries(mt_test PUBLIC gtest)
    endif ()

    # add_test(NAME ${CMAKE_PROJECT_NAME}_tst COMMAND mt_test WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
    gtest_discover_tests(mt_test WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
endif ()

# TODO: setup CPack

