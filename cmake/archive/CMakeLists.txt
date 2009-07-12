
project( libGrimArchive )


# Qt4 library
set( QT_MIN_VERSION "4.5.0" )
find_package( Qt4 REQUIRED )
set( QT_DONT_USE_QTGUI 1 )
include( ${QT_USE_FILE} )

find_package( ZLib REQUIRED )


# options
option( GRIM_ARCHIVE_DEBUG "Enable debugging for libGrimArchive" OFF )


# include directories
include_directories( "${CMAKE_CURRENT_BINARY_DIR}" )


# defines
add_definitions( -DGRIM_ARCHIVE_BUILD )
if ( GRIM_ARCHIVE_DEBUG )
	add_definitions( -DGRIM_ARCHIVE_DEBUG )
endif ( GRIM_ARCHIVE_DEBUG )


# paths
set( SRC "${GRIM_ROOT_DIR}/src/archive" )


# archive sources
set( grim_archive_HEADERS
	${SRC}/archive.h
	${SRC}/archive_p.h
	${SRC}/archivemanager.h
)

set( grim_archive_SOURCES
	${SRC}/archive.cpp
	${SRC}/archive_p.cpp
	${SRC}/archivefile.cpp
	${SRC}/archivemanager.cpp
)


# fill all grim sources
grim_add_sources( ${grim_archive_SOURCES} )


set( grim_archive_MOC_SOURCES )
qt4_wrap_cpp( grim_archive_MOC_SOURCES ${grim_archive_HEADERS} OPTIONS "-nw" )


# all source files
set( grim_archive_ALL_SOURCES ${grim_archive_SOURCES} ${grim_archive_MOC_SOURCES} )


add_library( libGrimArchive SHARED ${grim_archive_ALL_SOURCES} )
target_link_libraries( libGrimArchive ${QT_LIBRARIES} )
set_target_properties( libGrimArchive PROPERTIES VERSION "${GRIM_VERSION}" SOVERSION "${GRIM_SOVERSION}" OUTPUT_NAME GrimArchive )

grim_get_target_libraries( _interface_library _runtime_library libGrimArchive )
set( GRIM_ARCHIVE_LIBRARY "${_interface_library}" PARENT_SCOPE )

grim_add_precompiled_headers_wrapper( libGrimArchive "${SRC}/precompiledheaders.h" ${grim_archive_ALL_SOURCES} )


# installation
install( TARGETS libGrimArchive
	ARCHIVE DESTINATION "${GRIM_ARCHIVE_DESTINATION}"
	LIBRARY DESTINATION "${GRIM_LIBRARY_DESTINATION}"
	RUNTIME DESTINATION "${GRIM_RUNTIME_DESTINATION}"
	)
