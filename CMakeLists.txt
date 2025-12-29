cmake_minimum_required(VERSION 3.10)
project(dsi2lslGUI
	LANGUAGES C CXX
	VERSION 1.13.0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find an installed liblsl in paths set by the user (LSL_INSTALL_ROOT)
# and some default paths
find_package(LSL REQUIRED
	HINTS ${LSL_INSTALL_ROOT}
	"${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/build/"
	"${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/out/build/x64-Release"
	PATH_SUFFIXES share/LSL)
get_filename_component(LSL_PATH ${LSL_CONFIG} DIRECTORY)
message(STATUS "Found LSL lib in ${LSL_PATH}")
set(LSL_COMFY_DEFAULTS ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
# Set output directories for the build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)

# Sets variables for the directories used
# --------------------UPDATE base on your naming conventions, or updating version-------------------------------------------
SET(DSI-API "${CMAKE_CURRENT_SOURCE_DIR}/API/DSI_API_v1.20.3_06202025")
SET(LSL-CLI "${CMAKE_CURRENT_SOURCE_DIR}/CLI")
SET(LSL-GUI "${CMAKE_CURRENT_SOURCE_DIR}/GUI")
# --------------------------------------------------------------------------------------------------------------------------

# finds required packages
find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(Threads REQUIRED)

# creates the GUI .exe
add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32
	${LSL-GUI}/main.cpp
	${LSL-GUI}/mainwindow.cpp
	${LSL-GUI}/mainwindow.h
	${LSL-GUI}/mainwindow.ui
)

# creates the LSL wearbale sensing module .exe
add_executable(dsi2lsl 
    ${LSL-CLI}/dsi2lsl.c
    ${DSI-API}/DSI_API_Loader.c
	${DSI-API}/DSI.h
)


# the dependencies for LSL wearbale sensing module
target_link_libraries(dsi2lsl 
	PRIVATE
	LSL::lsl 
	Threads::Threads
)
target_include_directories(dsi2lsl 
	PRIVATE 
	"${DSI-API}" 
)

# the dependencies for the GUI
target_link_libraries(${PROJECT_NAME}
	PRIVATE
	Qt5::Widgets
	Threads::Threads
	LSL::lsl
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_14)

installLSLApp(${PROJECT_NAME})

LSLGenerateCPackConfig()
