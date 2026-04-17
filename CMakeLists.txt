cmake_minimum_required(VERSION 3.11)

project(MaterialFM LANGUAGES C CXX ASM)

add_subdirectory(vendor/psp-media-engine-custom-core)

if(DEFINED ENV{PSPDEV})
	set(ENGINE_PLATFORM "PSP")
else()
	message(FATAL_ERROR "Unsupported platform. Set PSPDEV to build PSP backend.")
endif()

set(ENGINE_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/platform/${ENGINE_PLATFORM}")
if(NOT EXISTS "${ENGINE_PLATFORM_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "Platform sources not defined for ${ENGINE_PLATFORM}")
endif()
include("${ENGINE_PLATFORM_DIR}/CMakeLists.txt")

include(cmake/MCU.cmake)
mcu_add_target(material-color-utilities)

add_compile_options(
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,GNU>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,15>>:-fdiagnostics-set-output=text:experimental-nesting=yes>"
	"$<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15>>:-fdiagnostics-set-output=text:experimental-nesting=yes>"
)

#set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -pg -g")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -pg -g")

add_executable(${PROJECT_NAME}
	src/main.cpp
	src/IO.cpp
	src/Application.cpp
	src/gui/Theme.cpp
	src/gui/Animation.cpp
	src/gui/Components.cpp
	src/gui/Context.cpp
	src/gui/IconAtlas.cpp
	src/gui/SystemDebug.cpp
	src/gui/SystemRender.cpp
	src/gui/System.cpp
	src/engine/Application.cpp
	src/engine/Asset.cpp
	src/engine/Implementations.cpp
	src/engine/Renderer.cpp
	${ENGINE_PLATFORM_SOURCES}
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

target_compile_options(${PROJECT_NAME} PRIVATE
	-Wall
	-Wextra
	-Wconversion
	-Wsign-conversion
	$<$<CONFIG:Debug>:-Og>
	$<$<CONFIG:Debug>:-g3>
)

set_source_files_properties(src/engine/Implementations.cpp PROPERTIES
	COMPILE_OPTIONS "-O3;-w;-fno-lto"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	ENGINE_PROJECT_NAME="${PROJECT_NAME}"
)

include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
if(lto_supported)
	set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	message(WARNING "LTO is not supported: ${lto_error}")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
	me-core
	material-color-utilities

	pspdebug
	pspdisplay
	pspge
	pspgum
	pspgu
	pspctrl
	pspaudio
	pspaudiolib
	pspmp3
)

target_include_directories(${PROJECT_NAME} PRIVATE include/)
target_include_directories(${PROJECT_NAME} PRIVATE src/)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/assets"
	COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets"
	COMMENT "Linking assets directory into build output"
)

create_pbp_file(
	TARGET ${PROJECT_NAME}
	ICON_PATH NULL
	BACKGROUND_PATH NULL
	PREVIEW_PATH NULL
	TITLE ${PROJECT_NAME}
	VERSION 01.00
)
