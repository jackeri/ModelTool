cmake_minimum_required(VERSION 3.10)
include(CheckIncludeFile)
include(ExternalProject)

project(ModelTool VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_TARGETS_FOLDER CMake)
set(BUNDLED_TARGETS_FOLDER Bundled)

option(test "Enable testing" ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ${CMAKE_TARGETS_FOLDER})

# Activate C++ 17 and C99 (if needed)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_C_STANDARD 99)

IF(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
	MESSAGE("No CMAKE_BUILD_TYPE specified, defaulting to ${CMAKE_BUILD_TYPE}")
ENDIF(NOT CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(DEBUG_MODE 1)
else()
  set(DEBUG_MODE 0)
endif()

# Setup static runtime linking in Windows (Msvc and Mingw)
if(MSVC)
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

	foreach(CompilerFlag ${CompilerFlags})
		string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()

	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT.lib /NODEFAULTLIB:MSVCRTD.lib")
	SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D_DEBUG")
	SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG")

	add_definitions(-D_CRT_SECURE_NO_WARNINGS) # Do not show CRT warnings
	add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)

	if(MSVC_IDE)
		if(MSVC_VERSION EQUAL 1700)
			set(CMAKE_GENERATOR_TOOLSET "v120_xp" CACHE STRING "Platform Toolset" FORCE)
		elseif(MSVC_VERSION EQUAL 1800)
			set(CMAKE_GENERATOR_TOOLSET "v120_xp" CACHE STRING "Platform Toolset" FORCE)
		elseif(MSVC_VERSION EQUAL 1910)
			set(CMAKE_GENERATOR_TOOLSET "v141_xp" CACHE STRING "Platform Toolset" FORCE)
		endif()
	endif(MSVC_IDE)
elseif(MINGW AND CMAKE_BUILD_TYPE MATCHES Release)
	set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -static-libgcc -static")
	set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static-libgcc -static-libstdc++ -static")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -s -static")
	set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS} -static-libgcc -liconv -s -static")
	set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} -static-libgcc -static-libstdc++ -liconv -s -static")
	add_definitions(-D_WIN32_IE=0x0501)
endif()

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

FILE(GLOB MODEL_TOOL_SRC
	"src/*.*"
	"src/*/*.*"
)

list(APPEND MODEL_TOOL_SRC ${MODEL_TOOL_SHADERS})
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
set(NFD_LIB "libs/nativefiledialog")
FILE(GLOB NFD_SRC
	${NFD_LIB}/src/common.h
	${NFD_LIB}/src/nfd_common.c
	${NFD_LIB}/src/nfd_common.h
)

if(WIN32)
	LIST(APPEND NFD_SRC "${NFD_LIB}/src/nfd_win.cpp")
elseif(APPLE)
	LIST(APPEND NFD_SRC "${NFD_LIB}/src/nfd_cocoa.m")
else()
	LIST(APPEND NFD_SRC "${NFD_LIB}/src/nfd_gtk.c")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	include_directories(${GTK3_INCLUDE_DIRS})
	link_directories(${GTK3_LIBRARY_DIRS})
	# Add other flags to the compiler
	add_definitions(${GTK3_CFLAGS_OTHER})
endif()

include_directories(${NFD_LIB}/src/include)
add_library(bundled_nfd ${NFD_SRC})

IF(UNIX AND NOT APPLE)
	target_link_libraries(bundled_nfd ${GTK3_LIBRARIES})
ENDIF()

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

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/glfw)

add_definitions(-DGLEW_STATIC)

include(libs/glew-2.1.0/CMakeLists.txt)

# Add this when we need it
#add_subdirectory(libs/glm)

include_directories(libs/glew-2.1.0/include)
include_directories(libs/glm)

set(MTLINK_LIBRARIES glew ${OPENGL_LIBRARIES} glfw bundled_minizip bundled_zlib bundled_nfd)

add_executable(ModelTool WIN32 MACOSX_BUNDLE ${MODEL_TOOL_SRC} ${IMGUI_SRC} ${LIB_HEADERS})
add_dependencies(ModelTool glew glfw)
target_link_libraries(ModelTool ${MTLINK_LIBRARIES})

set_target_properties(glew PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(glfw PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(bundled_minizip PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(bundled_zlib PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
set_target_properties(bundled_nfd PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ModelTool)

set_target_properties(ModelTool
	PROPERTIES COMPILE_DEFINITIONS ""
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

	add_subdirectory(libs/googletest)

	file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false
		"tst/*.h"
		"tst/*.cpp"
	)

	set(SOURCES ${TEST_SOURCES} ${MODEL_TOOL_SRC} ${IMGUI_SRC} ${LIB_HEADERS})

	add_executable(tst ${TEST_SOURCES})
	add_dependencies(tst ModelTool)
	target_include_directories(tst PRIVATE "src/")
	target_link_libraries(tst PUBLIC ${MTLINK_LIBRARIES} gtest)

	add_test(NAME ${CMAKE_PROJECT_NAME}_tst COMMAND tst)
endif()

# TODO: setup CPack

