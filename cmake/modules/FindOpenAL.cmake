# Locate OpenAL
# This module defines
# OPENAL_LIBRARY
# OPENAL_FOUND, if false, do not try to link to OpenAL 
# OPENAL_INCLUDE_DIR, where to find the headers
#
# $OPENALDIR is an environment variable that would
# correspond to the ./configure --prefix=$OPENALDIR
# used in building OpenAL.
#
# Created by Eric Wing. This was influenced by the FindSDL.cmake module.

# This makes the presumption that you are include al.h like
# #include "al.h"
# and not 
# #include <AL/al.h>
# The reason for this is that the latter is not entirely portable.
# Windows/Creative Labs does not by default put their headers in AL/ and 
# OS X uses the convention <OpenAL/al.h>.
# 
# For Windows, Creative Labs seems to have added a registry key for their 
# OpenAL 1.1 installer. I have added that key to the list of search paths,
# however, the key looks like it could be a little fragile depending on 
# if they decide to change the 1.00.0000 number for bug fix releases.
# Also, they seem to have laid down groundwork for multiple library platforms
# which puts the library in an extra subdirectory. Currently there is only
# Win32 and I have hardcoded that here. This may need to be adjusted as 
# platforms are introduced.
# The OpenAL 1.0 installer doesn't seem to have a useful key I can use.
# I do not know if the Nvidia OpenAL SDK has a registry key.
# 
# For OS X, remember that OpenAL was added by Apple in 10.4 (Tiger). 
# To support the framework, I originally wrote special framework detection 
# code in this module which I have now removed with CMake's introduction
# of native support for frameworks.
# In addition, OpenAL is open source, and it is possible to compile on Panther. 
# Furthermore, due to bugs in the initial OpenAL release, and the 
# transition to OpenAL 1.1, it is common to need to override the built-in
# framework. 
# Per my request, CMake should search for frameworks first in
# the following order:
# ~/Library/Frameworks/OpenAL.framework/Headers
# /Library/Frameworks/OpenAL.framework/Headers
# /System/Library/Frameworks/OpenAL.framework/Headers
#
# On OS X, this will prefer the Framework version (if found) over others.
# People will have to manually change the cache values of 
# OPENAL_LIBRARY to override this selection or set the CMake environment
# CMAKE_INCLUDE_PATH to modify the search paths.


# OPENAL_INCLUDE_DIR
# OPENAL_INCLUDE_DIR_PREFIX
# OPENAL_LIBRARY


set( OpenAL_FOUND NO )


set( _openal_pathes
	"${OPENAL_ROOT_DIR}/include"
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local
	/usr
	/sw # Fink
	/opt/local # DarwinPorts
	/opt/csw # Blastwave
	/opt
	[HKEY_LOCAL_MACHINE\\SOFTWARE\\Creative\ Labs\\OpenAL\ 1.1\ Software\ Development\ Kit\\1.00.0000;InstallDir]
)


if ( NOT OPENAL_INCLUDE_DIR )

	# normal path
	find_path( OPENAL_INCLUDE_DIR
		NAMES
			"AL/al.h"
		HINTS
			"$ENV{OPENALDIR}"
		PATH_SUFFIXES
			"include"
		PATHS
			${_openal_pathes}
		DOC
			"Path to OpenAL include directory"
	)

	if ( OPENAL_INCLUDE_DIR )
		set( OPENAL_INCLUDE_DIR_PREFIX "AL" )
	else ( OPENAL_INCLUDE_DIR )

		# Mac OS X SDK path
		find_path( OPENAL_INCLUDE_DIR
			NAMES
				"OpenAL/al.h"
			HINTS
				"$ENV{OPENALDIR}"
			PATH_SUFFIXES
				"include"
			PATHS
				${_openal_pathes}
		)

		if ( OPENAL_INCLUDE_DIR )
			set( OPENAL_INCLUDE_DIR_PREFIX "OpenAL" )
		else ( OPENAL_INCLUDE_DIR )
			# Creative Labs SDK path
			find_path( OPENAL_INCLUDE_DIR
				NAMES
					"al.h"
				HINTS
					"$ENV{OPENALDIR}"
				PATH_SUFFIXES
					"include"
				PATHS
					${_openal_pathes}
			)

			if ( OPENAL_INCLUDE_DIR )
				set( OPENAL_INCLUDE_DIR_PREFIX "Empty" )
			endif ( OPENAL_INCLUDE_DIR )
		endif ( OPENAL_INCLUDE_DIR )

	endif ( OPENAL_INCLUDE_DIR )

endif ( NOT OPENAL_INCLUDE_DIR )


find_library( OPENAL_LIBRARY 
	NAMES
		OpenAL al openal OpenAL32
	HINTS
		"$ENV{OPENALDIR}"
	PATH_SUFFIXES
		lib64 lib libs64 libs libs/Win32 libs/Win64
	PATHS
		"${OPENAL_ROOT_DIR}"
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local
		/usr
		/sw
		/opt/local
		/opt/csw
		/opt
		[HKEY_LOCAL_MACHINE\\SOFTWARE\\Creative\ Labs\\OpenAL\ 1.1\ Software\ Development\ Kit\\1.00.0000;InstallDir]
	DOC
		"Path to OpenAL library"
)

set( OPENAL_INCLUDE_DIR_PREFIX "${OPENAL_INCLUDE_DIR_PREFIX}" CACHE STRING "Prefix for OpenAL include directory" FORCE )
mark_as_advanced( FORCE OPENAL_INCLUDE_DIR_PREFIX )


if( OPENAL_LIBRARY AND OPENAL_INCLUDE_DIR )
	set( OpenAL_FOUND YES )
endif( OPENAL_LIBRARY AND OPENAL_INCLUDE_DIR )


set( _advanced_variables OPENAL_ROOT_DIR OPENAL_INCLUDE_DIR OPENAL_LIBRARY )

if ( NOT OpenAL_FOUND )
	set( OPENAL_ROOT_DIR "" CACHE PATH "Root directory for OpenAL library" )

	set( _message_common
		"\nOpenAL library not found.\nPlease specify OPENAL_ROOT_DIR variable or OPENAL_INCLUDE_DIR and OPENAL_LIBRARY separately." )
	if ( OpenAL_FIND_REQUIRED )
		mark_as_advanced( CLEAR ${_advanced_variables} )
		message( FATAL_ERROR
			"${_message_common}\n" )
	else ( OpenAL_FIND_REQUIRED )
		mark_as_advanced( ${_advanced_variables} )
		message(
			"${_message_common}\n"
			"You will find this variables in the advanced variables list." )
	endif ( OpenAL_FIND_REQUIRED )
else ( NOT OpenAL_FOUND )
	mark_as_advanced( FORCE ${_advanced_variables} )
	include_directories( ${OPENAL_INCLUDE_DIR} )
endif ( NOT OpenAL_FOUND )
