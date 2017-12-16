# Once done these will be defined:
#
#  LUAJIT_FOUND
#  LUAJIT_INCLUDE_DIRS
#  LUAJIT_LIBRARIES
#
# For use in OBS: 
#
#  LUAJIT_INCLUDE_DIR

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_LUAJIT QUIET LUAJIT)
ENDIF()

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET(_LIB_SUFFIX 64)
ELSE()
	SET(_LIB_SUFFIX 32)
ENDIF()

FIND_PATH(LUAJIT_INCLUDE_DIR
	NAMES lua.h
	HINTS
		ENV LuajitPath${_LIB_SUFFIX}
		ENV LuajitPath
		ENV DepsPath${_LIB_SUFFIX}
		ENV DepsPath
		${LuajitPath${_LIB_SUFFIX}}
		${LuajitPath}
		${DepsPath${_LIB_SUFFIX}}
		${DepsPath}
		${_LUAJIT_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	PATH_SUFFIXES
		include luajit luajit/src include/luajit include/luajit/src)

find_library(LUAJIT_LIB
	NAMES ${_LUAJIT_LIBRARIES} luajit lua
	HINTS
		ENV LuajitPath${_lib_suffix}
		ENV LuajitPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${LuajitPath${_lib_suffix}}
		${LuajitPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_LUAJIT_LIBRARY_DIRS}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib /sw/lib
	PATH_SUFFIXES
		lib${_lib_suffix} lib
		libs${_lib_suffix} libs
		bin${_lib_suffix} bin
		../lib${_lib_suffix} ../lib
		../libs${_lib_suffix} ../libs
		../bin${_lib_suffix} ../bin)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Luajit DEFAULT_MSG LUAJIT_LIB LUAJIT_INCLUDE_DIR)
mark_as_advanced(LUAJIT_INCLUDE_DIR LUAJIT_LIB)

if(LUAJIT_FOUND)
	set(LUAJIT_INCLUDE_DIRS ${LUAJIT_INCLUDE_DIR})
	set(LUAJIT_LIBRARIES ${LUAJIT_LIB})
endif()
