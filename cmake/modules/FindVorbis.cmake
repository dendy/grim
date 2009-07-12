
# Looks up for the Vorbis libraries
#
# Possible environment variables:
#
# VORBIS - points where Vorbis root directory exists
#
# Outputs:
#
# VORBIS_INLUDE_DIR
# VORBIS_LIBRARY
# VORBISFILE_LIBRARY


set( Vorbis_FOUND NO )


find_path( VORBIS_INCLUDE_DIR
	NAMES
		"vorbis/codec.h"
		"vorbis/vorbisenc.h"
		"vorbis/vorbisfile.h"
	PATHS
		"${VORBIS_ROOT_DIR}/include"
		"$ENV{VORBISDIR}/include"
		"/usr/include"
	DOC
		"Path to Vorbis include directory"
)


if ( WIN32 )
	if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
		set( _vorbis_lib_names "vorbis_d" "vorbis" "libvorbis" )
		set( _vorbisfile_lib_names "vorbisfile_d" "vorbisfile" "libvorbisfile" )
		set( _vorbis_path_suffixes
			"win32/Vorbis_Dynamic_Debug"
			"win32/VorbisFile_Dynamic_Debug"
			"win32/VS2008/Win32/Debug"
			"win32/VS2005/Win32/Debug"
			)
	else ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
		set( _vorbis_lib_names "vorbis" "libvorbis" )
		set( _vorbisfile_lib_names "vorbisfile" "libvorbisfile" )
		set( _vorbis_path_suffixes
			"win32/Vorbis_Dynamic_Release"
			"win32/VorbisFile_Dynamic_Release"
			"win32/VS2008/Win32/Release"
			"win32/VS2005/Win32/Release"
			)
	endif ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
else ( WIN32 )
		set( _vorbis_lib_names "vorbis" "Vorbis" "VORBIS" )
		set( _vorbisfile_lib_names "vorbisfile" "VorbisFile" "VORBISFILE" )
endif ( WIN32 )

find_library( VORBIS_LIBRARY
	NAMES
		${_vorbis_lib_names}
	PATH_SUFFIXES
		${_vorbis_path_suffixes}
	PATHS
		"${VORBIS_ROOT_DIR}/lib"
		"${VORBIS_ROOT_DIR}"
		$ENV{VORBISDIR}/lib
		$ENV{VORBISDIR}
		$ENV{OGGDIR}/lib
		$ENV{OGGDIR}
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
	DOC
		"Path to Vorbis library"
)

find_library( VORBISFILE_LIBRARY
	NAMES
		${_vorbisfile_lib_names}
	PATH_SUFFIXES
		${_vorbis_path_suffixes}
	PATHS
		"${VORBIS_ROOT_DIR}/lib"
		"${VORBIS_ROOT_DIR}"
		$ENV{VORBISDIR}/lib
		$ENV{VORBISDIR}
		$ENV{OGGDIR}/lib
		$ENV{OGGDIR}
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
	DOC
		"Path to VorbisFile library"
)


if ( VORBIS_INCLUDE_DIR AND VORBIS_LIBRARY AND VORBISFILE_LIBRARY )
	set( Vorbis_FOUND YES )
endif ( VORBIS_INCLUDE_DIR AND VORBIS_LIBRARY AND VORBISFILE_LIBRARY )



set( _advanced_variables VORBIS_ROOT_DIR VORBIS_INCLUDE_DIR VORBIS_LIBRARY VORBISFILE_LIBRARY )

if ( NOT Vorbis_FOUND )
	set( VORBIS_ROOT_DIR "" CACHE PATH "Root directory for Vorbis library" )

	set( _message_common
		"\nVorbis library not found.\nPlease specify VORBIS_ROOT_DIR variable or VORBIS_INCLUDE_DIR and VORBIS_LIBRARY and VORBISFILE_LIBRARY separately." )
	if ( Vorbis_FIND_REQUIRED )
		mark_as_advanced( CLEAR ${_advanced_variables} )
		message( FATAL_ERROR
			"${_message_common}\n" )
	else ( Vorbis_FIND_REQUIRED )
		mark_as_advanced( ${_advanced_variables} )
		message(
			"${_message_common}\n"
			"You will find this variables in the advanced variables list." )
	endif ( Vorbis_FIND_REQUIRED )
else ( NOT Vorbis_FOUND )
	mark_as_advanced( FORCE ${_advanced_variables} )
	include_directories( ${VORBIS_INCLUDE_DIR} )
endif ( NOT Vorbis_FOUND )
