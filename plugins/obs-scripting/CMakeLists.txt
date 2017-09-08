cmake_minimum_required(VERSION 2.8)
project(obs-scripting)

if(MSVC)
	set(obs-scripting_PLATFORM_DEPS
		w32-pthreads)
endif()

set(Python_ADDITIONAL_VERSIONS 3.4)
find_package(PythonLibs QUIET 3.4)
find_package(SWIG QUIET 2)

set(LUA_FOUND TRUE)

if(NOT SWIG_FOUND)
	message(STATUS "Scripting plugin: SWIG not found; scripting plugin disabled")
	return()
endif()

if(NOT PYTHONLIBS_FOUND AND NOT LUA_FOUND)
	message(STATUS "Scripting plugin: Neither Python 3 nor Lua 5 was found; scripting plugin disabled")
	return()
endif()

if(NOT PYTHONLIBS_FOUND)
	message(STATUS "Scripting plugin: Python 3 not found; Python support disabled")
else()
	message(STATUS "Scripting plugin: Python 3 supported")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/obs-scripting-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/obs-scripting-config.h")

include(${SWIG_USE_FILE})

include_directories(${CMAKE_SOURCE_DIR}/libobs)
include_directories(${PYTHON_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${LUA_INCLUDE_DIR})

if(PYTHONLIBS_FOUND)
	set(obs-scripting-python_SOURCES
		obs-scripting-python.c
		obs-scripting-python-source.c
		)
	set(obs-scripting-python_HEADERS
		obs-scripting-python.h
		)
endif()

if(LUA_FOUND)
	set(obs-scripting-lua_SOURCES
		obs-scripting-lua.c
		obs-scripting-lua-source.c
		)
	set(obs-scripting-lua_HEADERS
		obs-scripting-lua.h
		)
endif()

set(obs-scripting_SOURCES
	obs-scripting-module.c
	cstrcache.cpp
	)
set(obs-scripting_HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/obs-scripting-config.h
	cstrcache.h
	)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/swig)

if(PYTHONLIBS_FOUND)
	set(SWIG_PY_RUNTIME swig/swigpyrun.h)
	add_custom_command(OUTPUT ${SWIG_PY_RUNTIME}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		PRE_BUILD
		COMMAND ${SWIG_EXECUTABLE} -python -external-runtime ${SWIG_PY_RUNTIME}
		COMMENT "Scripting plugin: Building Python SWIG interface header"
		)
	set_source_files_properties(${SWIG_PY_RUNTIME} PROPERTIES GENERATED TRUE)
endif()

if(LUA_FOUND)
	set(SWIG_LUA_RUNTIME swig/swigluarun.h)
	add_custom_command(OUTPUT ${SWIG_LUA_RUNTIME}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		PRE_BUILD
		COMMAND ${SWIG_EXECUTABLE} -lua -external-runtime ${SWIG_LUA_RUNTIME}
		COMMENT "Scripting plugin: Building Lua SWIG interface header"
		)
	set_source_files_properties(${SWIG_LUA_RUNTIME} PROPERTIES GENERATED TRUE)
endif()

add_library(obs-scripting SHARED
	${obs-scripting_SOURCES}
	${obs-scripting_HEADERS}
	${obs-scripting-python_SOURCES}
	${obs-scripting-python_HEADERS}
	${obs-scripting-lua_SOURCES}
	${obs-scripting-lua_HEADERS}
	${SWIG_PY_RUNTIME}
	${SWIG_LUA_RUNTIME}
	)

string(REGEX REPLACE "_d" "" PYTHON_LIBRARIES "${PYTHON_LIBRARIES}")

target_link_libraries(obs-scripting
	libobs
	lua-embedded
	${PYTHON_LIBRARIES}
	${obs-scripting_PLATFORM_DEPS}
	)

if(PYTHONLIBS_FOUND)
	add_subdirectory(obspython)
endif()

if(LUA_FOUND)
	add_subdirectory(obslua)
endif()

install_obs_plugin_with_data(obs-scripting data)
