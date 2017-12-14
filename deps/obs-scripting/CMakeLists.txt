cmake_minimum_required(VERSION 2.8)
project(obs-scripting)

if(MSVC)
	set(obs-scripting_PLATFORM_DEPS
		w32-pthreads)
endif()

find_package(Lua QUIET 5.1)
find_package(SWIG QUIET 2)

if(NOT SWIG_FOUND)
	message(STATUS "Scripting: SWIG not found; scripting disabled")
	return()
endif()

if(NOT LUA_FOUND)
	message(STATUS "Scripting: Lua not found; Lua support disabled")
	return()
else()
	message(STATUS "Scripting: Lua supported")
endif()

if(UI_ENABLED)
	message(STATUS "test123")
	set(EXTRA_LIBS obs-frontend-api)
	include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/obs-scripting-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/obs-scripting-config.h")

include(${SWIG_USE_FILE})

include_directories(${CMAKE_SOURCE_DIR}/libobs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${LUA_INCLUDE_DIR})

if(LUA_FOUND)
	set(obs-scripting-lua_SOURCES
		obs-scripting-lua.c
		obs-scripting-lua-source.c
		)
	set(obs-scripting-lua_HEADERS
		obs-scripting-lua.h
		)
	if(UI_ENABLED)
		set(obs-scripting-lua_SOURCES
			${obs-scripting-lua_SOURCES}
			obs-scripting-lua-frontend.c
			)
	endif()
endif()

set(obs-scripting_SOURCES
	obs-scripting.c
	cstrcache.cpp
	)
set(obs-scripting_HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/obs-scripting-config.h
	obs-scripting.h
	cstrcache.h
	)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/swig)

if(LUA_FOUND)
	set(SWIG_LUA_RUNTIME swig/swigluarun.h)
	add_custom_command(OUTPUT ${SWIG_LUA_RUNTIME}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		PRE_BUILD
		COMMAND ${SWIG_EXECUTABLE} -lua -external-runtime ${SWIG_LUA_RUNTIME}
		COMMENT "Scripting: Building Lua SWIG interface header"
		)
	set_source_files_properties(${SWIG_LUA_RUNTIME} PROPERTIES GENERATED TRUE)
endif()

add_library(obs-scripting SHARED
	${obs-scripting_SOURCES}
	${obs-scripting_HEADERS}
	${obs-scripting-lua_SOURCES}
	${obs-scripting-lua_HEADERS}
	${SWIG_LUA_RUNTIME}
	)

target_link_libraries(obs-scripting
	libobs
	${LUA_LIBRARIES}
	${EXTRA_LIBS}
	${obs-scripting_PLATFORM_DEPS}
	)

if(LUA_FOUND)
	add_subdirectory(obslua)
endif()

install_obs_core(obs-scripting)
