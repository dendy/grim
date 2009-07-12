#
# Find ZLib for using with Qt4
#
# Normally Qt4 already bundled with ZLib.
# All we have to search is include path.
#
# Outputs:
#
# ZLIB_INCLUDE_DIR


set( ZLib_FOUND NO )


if ( WIN32 )
	find_path( ZLIB_INCLUDE_DIR zlib.h
		PATHS
			"$ENV{QTDIR}"
			"${QT_INCLUDE_DIR}/.."
		PATH_SUFFIXES
			"src/3rdparty/zlib"
	)

	if ( ZLIB_INCLUDE_DIR )
		include_directories( ${ZLIB_INCLUDE_DIR} )
		set( ZLib_FOUND YES )
		mark_as_advanced( FORCE ZLIB_INCLUDE_DIR )
	else ( ZLIB_INCLUDE_DIR )
		mark_as_advanced( CLEAR ZLIB_INCLUDE_DIR )
	endif ( ZLIB_INCLUDE_DIR )
endif ( WIN32 )
