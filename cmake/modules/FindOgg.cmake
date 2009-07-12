
# Looks up for the Ogg libraries
#
# Possible environment variables:
#
# OGGDIR - points where Ogg root directory exists
#
# Outputs:
#
# OGG_INLUDE_DIR
# OGG_LIBRARY


set( Ogg_FOUND NO )


find_path( OGG_INCLUDE_DIR
	NAMES
		"ogg/ogg.h"
	PATHS
		"${OGG_ROOT_DIR}/include"
		"$ENV{OGGDIR}"
		"/usr/include"
	DOC
		"Path to Ogg include directory"
)

if ( WIN32 )
	if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
		set( _ogg_lib_names "ogg_d" "ogg" "libogg" )
		set( _ogg_path_suffixes
			"win32/Dynamic_Debug"
			"win32/VS2008/Win32/Debug"
			"win32/VS2005/Win32/Debug"
			"win32/VS2003/Debug"
			)
	else ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
		set( _ogg_lib_names "ogg" "libogg" )
		set( _ogg_path_suffixes
			"win32/Dynamic_Release"
			"win32/VS2008/Win32/Release"
			"win32/VS2005/Win32/Release"
			"win32/VS2003/Release"
			)
	endif ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
else ( WIN32 )
	set( _ogg_lib_names "ogg" "Ogg" "OGG" )
	set( _ogg_path_suffixes "lib" "lib64" )
endif ( WIN32 )

find_library( OGG_LIBRARY
	NAMES
		${_ogg_lib_names}
	PATH_SUFFIXES
		${_ogg_path_suffixes}
	PATHS
		"${OGG_ROOT_DIR}"
		$ENV{OGGDIR}
		$ENV{VORBISDIR}
		/usr
		/usr/local
		/sw
		/opt
		/opt/local
		/opt/csw
	DOC
		"Path to Ogg library"
)


if ( OGG_INCLUDE_DIR AND OGG_LIBRARY )
	set( Ogg_FOUND YES )
endif ( OGG_INCLUDE_DIR AND OGG_LIBRARY )


set( _advanced_variables OGG_ROOT_DIR OGG_INCLUDE_DIR OGG_LIBRARY )

if ( NOT Ogg_FOUND )
	set( OGG_ROOT_DIR "" CACHE PATH "Root directory for Ogg library" )

	set( _message_common
		"\nOgg library not found.\nPlease specify OGG_ROOT_DIR variable or OGG_INCLUDE_DIR and OGG_LIBRARY separately." )
	if ( Ogg_FIND_REQUIRED )
		mark_as_advanced( CLEAR ${_advanced_variables} )
		message( FATAL_ERROR
			"${_message_common}\n" )
	else ( Ogg_FIND_REQUIRED )
		mark_as_advanced( ${_advanced_variables} )
		message(
			"${_message_common}\n"
			"You will find this variables in the advanced variables list.\n" )
	endif ( Ogg_FIND_REQUIRED )
else ( NOT Ogg_FOUND )
	mark_as_advanced( FORCE ${_advanced_variables} )
	include_directories( ${OGG_INCLUDE_DIR} )
endif ( NOT Ogg_FOUND )
