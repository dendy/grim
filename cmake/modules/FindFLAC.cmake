
# Looks up for the FLAC libraries
#
# Possible environment variables:
#
# FLACDIR - points where FLAC root directory exists
#
# Outputs:
#
# FLAC_INLUDE_DIR
# FLAC_LIBRARY


set( FLAC_FOUND NO )


find_path( FLAC_INCLUDE_DIR
	NAMES
		"FLAC/all.h"
	PATHS
		"${FLAC_ROOT_DIR}/include"
		"$ENV{FLACDIR}"
		"$ENV{FLACDIR}/include"
		"/usr/include"
	DOC
		"Path to FLAC include directory"
)


if ( WIN32 )
	if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
		set( _flac_path_suffixes "obj/debug/lib" )
	else ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
		set( _flac_path_suffixes "obj/release/lib" )
	endif ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
endif ( WIN32 )


find_library( FLAC_LIBRARY
	NAMES
		flac FLAC libFLAC_dynamic
	PATH_SUFFIXES
		${_flac_path_suffixes}
	PATHS
		"${FLAC_ROOT_DIR}"
		"${FLAC_ROOT_DIR}/lib"
		"$ENV{FLACDIR}"
		"$ENV{FLACDIR}/lib"
		"/usr/local/lib"
		"/usr/lib"
		"/sw/lib"
		"/opt/local/lib"
		"/opt/csw/lib"
		"/opt/lib"
	DOC
		"Path to FLAC library"
)


if ( FLAC_INCLUDE_DIR AND FLAC_LIBRARY )
	set( FLAC_FOUND YES )
endif ( FLAC_INCLUDE_DIR AND FLAC_LIBRARY )


set( _advanced_variables FLAC_ROOT_DIR FLAC_INCLUDE_DIR FLAC_LIBRARY )

if ( NOT FLAC_FOUND )
	set( FLAC_ROOT_DIR "" CACHE PATH "Root directory for FLAC library" )

	set( _message_common
		"\nFLAC library not found.\nPlease specify FLAC_ROOT_DIR variable or FLAC_INCLUDE_DIR and FLAC_LIBRARY separately." )
	if ( FLAC_FIND_REQUIRED )
		mark_as_advanced( CLEAR ${_advanced_variables} )
		message( FATAL_ERROR
			"${_message_common}\n" )
	else ( FLAC_FIND_REQUIRED )
		mark_as_advanced( ${_advanced_variables} )
		message(
			"${_message_common}\n"
			"You will find this variables in the advanced variables list." )
	endif ( FLAC_FIND_REQUIRED )
else ( NOT FLAC_FOUND )
	mark_as_advanced( FORCE ${_advanced_variables} )
	include_directories( ${FLAC_INCLUDE_DIR} )
endif ( NOT FLAC_FOUND )
