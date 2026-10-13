cmake_minimum_required(VERSION 3.30)
include(CheckIncludeFile)
include(ExternalProject)

# The version comes from the latest v<major>.<minor>.<patch> git tag
include(cmake/git_version.cmake)
project(ModelTool VERSION ${MODELTOOL_VERSION} LANGUAGES C CXX)

set(CMAKE_TARGETS_FOLDER CMake)
set(BUNDLED_TARGETS_FOLDER Bundled)

option(test "Enable testing" ON)
option(NATIVE_DIALOG "Use platform native dialog" OFF)

# Enable the generation of compile_commands.json by default
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    # Static C runtime for every target (bundled libraries too), so the program runs without the Visual C++
    # redistributable. CMake 3.15+ picks the runtime with this variable, not with /MD /MT in the compiler flags.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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

# Remove main.cpp from the list of files to compile (for testing)
list(FILTER MODEL_TOOL_APP_SRC EXCLUDE REGEX ".*/main.cpp$")
list(FILTER MODEL_TOOL_LIB_SRC EXCLUDE REGEX ".*/main.cpp$")
list(FILTER MODEL_TOOL_CLI_SRC EXCLUDE REGEX ".*/main.cpp$")

list(APPEND MODEL_TOOL_APP_SRC ${MODEL_TOOL_SHADERS})

# The UI fonts are built into the program (src/app/embedded_files.h)
set(MODEL_TOOL_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Hack-Regular.ttf" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/fontawesome-webfont.ttf")
set(MODEL_TOOL_EMBEDDED "${CMAKE_BINARY_DIR}/generated/embedded_files.cpp")
string(REPLACE ";" "|" MODEL_TOOL_FONTS_ARG "${MODEL_TOOL_FONTS}")
add_custom_command(
        OUTPUT ${MODEL_TOOL_EMBEDDED}
        COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${MODEL_TOOL_EMBEDDED}" "-DFILES=${MODEL_TOOL_FONTS_ARG}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_files.cmake"
        DEPENDS ${MODEL_TOOL_FONTS} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_files.cmake"
        COMMENT "Embedding the fonts"
        VERBATIM)
add_custom_target(embedded_files DEPENDS ${MODEL_TOOL_EMBEDDED})
set_source_files_properties(${MODEL_TOOL_EMBEDDED} PROPERTIES GENERATED TRUE INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/app")
list(APPEND MODEL_TOOL_APP_SRC ${MODEL_TOOL_EMBEDDED})
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

set(MINIZIP_LIB "libs/zlib/contrib/minizip")

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
        "libs/imguizmo/ImGuizmo.cpp"
        "libs/imguizmo/ImGuizmo.h"
        # Backends
        "libs/imgui/backends/imgui_impl_glfw.cpp"
        "libs/imgui/backends/imgui_impl_opengl3.cpp"
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
include_directories("libs/imguizmo")
include_directories("libs/stb")

# tinygltf v3 (C11 glTF / GLB parser). Its own CMake file only builds tests, so the library target is made here.
add_library(bundled_tinygltf STATIC "libs/tinygltf/tiny_gltf_v3.c" "libs/tinygltf/tiny_gltf_v3.h")
target_include_directories(bundled_tinygltf PUBLIC "libs/tinygltf")
set_target_properties(bundled_tinygltf PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF FOLDER ${BUNDLED_TARGETS_FOLDER})

# ufbx (vendored single file library, FBX and OBJ loading)
add_library(bundled_ufbx STATIC "libs/ufbx/ufbx.c" "libs/ufbx/ufbx.h")
target_include_directories(bundled_ufbx PUBLIC "libs/ufbx")
set_target_properties(bundled_ufbx PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE) # static C runtime like the rest (MSVC)
add_subdirectory(libs/glfw)

add_subdirectory(libs/spdlog)

add_subdirectory(libs/glad)

# Add this when we need it
# add_subdirectory(libs/glm)

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE libs/glm)


add_library(ModelToolLib STATIC ${MODEL_TOOL_LIB_SRC})
target_include_directories(ModelToolLib PUBLIC "src/lib")

# mt_version.h: the version for the programs (window title, mt --version)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/mt_version.h.in" "${CMAKE_BINARY_DIR}/generated/mt_version.h" @ONLY)
target_include_directories(ModelToolLib PUBLIC "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(ModelToolLib PUBLIC glm bundled_minizip PRIVATE bundled_tinygltf bundled_ufbx)

add_executable(ModelToolCLI "src/cli/main.cpp" ${MODEL_TOOL_CLI_SRC})
set_target_properties(ModelToolCLI PROPERTIES OUTPUT_NAME "mt")
target_link_libraries(ModelToolCLI ModelToolLib)

# Test data generator (tools/fixtures), not part of the normal build: cmake --build build --target generate_fixtures
add_executable(generate_fixtures EXCLUDE_FROM_ALL "tools/fixtures/generate_fixtures.cpp")
target_link_libraries(generate_fixtures ModelToolLib)

set(MTLINK_LIBRARIES ModelToolLib glm glad ${OPENGL_LIBRARIES} glfw bundled_minizip bundled_zlib bundled_nfd spdlog)
# Application icon
set(MODEL_TOOL_RESOURCES "")
if (APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "icon.icns")
    set(MACOSX_BUNDLE_BUNDLE_NAME "ModelTool")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.jackeri.ModelTool")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MODEL_TOOL_ICON "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.icns")
    set_source_files_properties(${MODEL_TOOL_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND MODEL_TOOL_RESOURCES ${MODEL_TOOL_ICON})
elseif (WIN32)
    list(APPEND MODEL_TOOL_RESOURCES "res/modeltool.rc")
endif ()

add_executable(ModelTool WIN32 MACOSX_BUNDLE "src/app/main.cpp" ${MODEL_TOOL_APP_SRC} ${IMGUI_SRC} ${LIB_HEADERS} ${MODEL_TOOL_RESOURCES})
add_dependencies(ModelTool glad glfw spdlog embedded_files)
if (APPLE)
    # CMake's default template plus NSHighResolutionCapable for sharp text on Retina screens
    set_target_properties(ModelTool PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/res/Info.plist.in")
endif ()
target_link_libraries(ModelTool ${MTLINK_LIBRARIES})

set_target_properties(glad PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(glfw PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(spdlog PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(bundled_minizip PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(bundled_zlib PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})


set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ModelTool)

set_target_properties(ModelTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ""
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ""
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ""
)

install(TARGETS ModelTool BUNDLE DESTINATION "." RUNTIME DESTINATION ".")
install(TARGETS ModelToolCLI RUNTIME DESTINATION ".")
install(FILES "LICENSE.txt" DESTINATION ".")


if (test)

    enable_testing()
    include(GoogleTest)

    # Try to use the system gtest package is exists
    find_package(GTest)

    if (NOT GTest_FOUND)
        # Only needed for the tests, keep it out of the install and the packages
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        add_subdirectory(libs/googletest)
    endif ()

    file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false
        "tst/*.h"
        "tst/*.cpp"
    )

    set(ALL_SOURCES ${TEST_SOURCES} ${MODEL_TOOL_APP_SRC} ${MODEL_TOOL_CLI_SRC} ${IMGUI_SRC} ${LIB_HEADERS})

    add_executable(mt_test ${ALL_SOURCES})
    add_dependencies(mt_test ModelTool)
    target_include_directories(mt_test PRIVATE "src/app" "src/cli")
    target_link_libraries(mt_test PUBLIC ${MTLINK_LIBRARIES})

    if (GTest_FOUND)
        target_link_libraries(mt_test PUBLIC GTest::gtest GTest::gtest_main)
    else ()
        target_link_libraries(mt_test PUBLIC gtest)
    endif ()

    # add_test(NAME ${CMAKE_PROJECT_NAME}_tst COMMAND mt_test WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
    gtest_discover_tests(mt_test WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
endif ()

# Packaging: cpack -G ZIP (or DragNDrop on macOS, WIX on Windows, TGZ on Linux)
set(CPACK_PACKAGE_NAME "ModelTool")
set(CPACK_PACKAGE_VENDOR "ModelTool contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Model viewer and editor for Enemy Territory: Legacy")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
# Package names have the full version from git (1.2.3 on a tag, 1.2.3-4-gabcdef after it), unless it is set here
set(MODELTOOL_PACKAGE_VERSION "" CACHE STRING "Version in the package file names, empty for the git version")
if (MODELTOOL_PACKAGE_VERSION)
    set(package_version "${MODELTOOL_PACKAGE_VERSION}")
else ()
    set(package_version "${MODELTOOL_VERSION_FULL}")
endif ()
set(CPACK_PACKAGE_FILE_NAME "ModelTool-${package_version}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ModelTool")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_EXECUTABLES "ModelTool" "Model Tool")

if (APPLE)
    set(CPACK_GENERATOR "DragNDrop" "ZIP")
    set(CPACK_DMG_VOLUME_NAME "ModelTool")
elseif (WIN32)
    set(CPACK_GENERATOR "WIX" "ZIP")
    set(CPACK_WIX_UPGRADE_GUID "64F192A3-D882-4ADA-AC89-BA27002D8DD0")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/res/cpack_wix_ui_banner.jpg") # 493x58 px
    set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_SOURCE_DIR}/res/cpack_wix_ui_dialog.jpg") # 493x312 px
    set(CPACK_CREATE_DESKTOP_LINKS "ModelTool")
else ()
    set(CPACK_GENERATOR "TGZ")
endif ()

include(CPack)

