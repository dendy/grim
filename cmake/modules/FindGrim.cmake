
# Locates Grim library
#
# Output variables:
#
# GRIM_ROOT_DIR
# GRIM_INCLUDE_DIR
# GRIM_GAME_LIBRARY
# GRIM_ARCHIVE_LIBRARY
# GRIM_AUDIO_LIBRARY
# GRIM_TOOLS_LIBRARY


set( Grim_FOUND NO )


# list of Grim modules
set( _grim_modules
	"Game"
	"Archive"
	"Audio"
	"Tools"
	)


# if "All" token was given as REQUIRED then mark all Grim modules as REQUIRED
if ( Grim_FIND_REQUIRED_All )
	foreach( _module ${_grim_modules} )
		set( Grim_FIND_REQUIRED_${_module} ON )
	endforeach( _module ${_grim_modules} )
endif ( Grim_FIND_REQUIRED_All )


# possible paths and suffixes
set( _grim_path_suffixes "lib" )
if ( WIN32 )
	set( _grim_paths "${GRIM_ROOT_DIR}" )
else ( WIN32 )
	if ( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" )
		set( _grim_path_suffixes "lib64" ${_grim_path_suffixes} )
	endif ( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" )

	set( _grim_paths
		"${GRIM_ROOT_DIR}"
		/usr
		/usr/local
		/sw
		/opt
		/opt/local
		/opt/csw
		)
endif ( WIN32 )


# locate Macros.cmake
find_file( _macros_cmake
	NAMES
		"Macros.cmake"
	HINTS
		"${GRIM_ROOT_DIR}/cmake/modules"
	PATH_SUFFIXES
		"share/grim/cmake/modules"
	PATHS
		${_grim_paths}
)

if ( _macros_cmake )
	include( "${_macros_cmake}" )
endif ( _macros_cmake )

unset( _macros_cmake CACHE )


# locate GRIM_INCLUDE_DIR
find_path( GRIM_INCLUDE_DIR
	NAMES
		"grim/archive/archiveglobal.h"
		"grim/audio/audioglobal.h"
		"grim/tools/toolsglobal.h"
	HINTS
		"${GRIM_ROOT_DIR}/include"
	PATH_SUFFIXES
		"include"
	PATHS
		${_grim_paths}
	DOC
		"Path to Grim include directory"
)


# locate GRIM_GAME_LIBRARY
find_library( GRIM_GAME_LIBRARY
	NAMES
		"GrimGame"
	PATH_SUFFIXES
		${_grim_path_suffixes}
	PATHS
		${_grim_paths}
	DOC
		"Path to GrimGame library"
)

# locate GRIM_ARCHIVE_LIBRARY
find_library( GRIM_ARCHIVE_LIBRARY
	NAMES
		"GrimArchive"
	PATH_SUFFIXES
		${_grim_path_suffixes}
	PATHS
		${_grim_paths}
	DOC
		"Path to GrimArchive library"
)

# locate GRIM_AUDIO_LIBRARY
find_library( GRIM_AUDIO_LIBRARY
	NAMES
		"GrimAudio"
	PATH_SUFFIXES
		${_grim_path_suffixes}
	PATHS
		${_grim_paths}
	DOC
		"Path to GrimAudio library"
)

# locate GRIM_TOOLS_LIBRARY
find_library( GRIM_TOOLS_LIBRARY
	NAMES
		"GrimTools"
	PATH_SUFFIXES
		${_grim_path_suffixes}
	PATHS
		${_grim_paths}
	DOC
		"Path to GrimTools library"
)


# fill GRIM_LIBRARIES list
set( GRIM_LIBRARIES )
foreach( _module ${_grim_modules} )
	string( TOUPPER "${_module}" _module_upper )
	set( _library_variable "GRIM_${_module_upper}_LIBRARY" )

	if ( ${_library_variable} )
		list( APPEND GRIM_LIBRARIES "${${_library_variable}}" )
	endif ( ${_library_variable} )
endforeach( _module ${_grim_modules} )


set( _true ON )
while ( _true )
	# check for the GRIM_INCLUDE_DIR
	if ( NOT GRIM_INCLUDE_DIR )
		break()
	endif ( NOT GRIM_INCLUDE_DIR )

	# check that all REQUIRED modules were found
	set( _failed_to_locate_required )
	foreach( _module ${_grim_modules} )
		string( TOUPPER "${_module}" _module_upper )
		set( _library_variable "GRIM_${_module_upper}_LIBRARY" )

		if ( Grim_FIND_REQUIRED_${_module} AND NOT ${_library_variable} )
			set( _failed_to_locate_required YES )
			break()
		endif ( Grim_FIND_REQUIRED_${_module} AND NOT ${_library_variable} )
	endforeach( _module ${_grim_modules} )

	if ( _failed_to_locate_required )
		break()
	endif ( _failed_to_locate_required )

	# all requirements are satisfied
	set( Grim_FOUND YES )
	break()
endwhile ( _true )


# list of variables that allows user to fine tune where is what
set( _advanced_variables
	GRIM_ROOT_DIR
	GRIM_INCLUDE_DIR
	GRIM_GAME_LIBRARY
	GRIM_ARCHIVE_LIBRARY
	GRIM_AUDIO_LIBRARY
	GRIM_TOOLS_LIBRARY
	)


if ( NOT Grim_FOUND )
	set( GRIM_ROOT_DIR "" CACHE PATH "Root directory for Grim library" )

	set( _message_common "Grim library not found.\n" )
	set( _message_common
		"${_message_common}Please specify GRIM_ROOT_DIR variable or GRIM_INCLUDE_DIR and GRIM_GAME_LIBRARY and GRIM_ARCHIVE_LIBRARY and GRIM_AUDIO_LIBRARY and GRIM_TOOLS_LIBRARY separately." )
	if ( Grim_FIND_REQUIRED )
		mark_as_advanced( CLEAR ${_advanced_variables} )
		message( FATAL_ERROR
			"${_message_common}\n" )
	else ( Grim_FIND_REQUIRED )
		mark_as_advanced( ${_advanced_variables} )
		message(
			"${_message_common}\n"
			"You will find this variables in the advanced variables list.\n" )
	endif ( Grim_FIND_REQUIRED )
else ( NOT Grim_FOUND )
	include_directories( "${GRIM_INCLUDE_DIR}" )
	mark_as_advanced( FORCE ${_advanced_variables} )
endif ( NOT Grim_FOUND )
