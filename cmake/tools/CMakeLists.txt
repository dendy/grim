
project( libGrimTools )

# Qt4 library
set( QT_MIN_VERSION "4.5.0" )
find_package( Qt4 REQUIRED )
include( ${QT_USE_FILE} )


# options
option( GRIM_TOOLS_DEBUG "Enable debugging for libGrimTools" OFF )


# include directories
include_directories( "${CMAKE_CURRENT_BINARY_DIR}" )


# defines
add_definitions( -DGRIM_TOOLS_BUILD )
if ( GRIM_TOOLS_DEBUG )
	add_definitions( -DGRIM_TOOLS_DEBUG )
endif ( GRIM_TOOLS_DEBUG )


# paths
set( SRC "${GRIM_ROOT_DIR}/src/tools" )


# archive sources
set( grim_tools_HEADERS
#	${SRC}/displaymodeswitcher.h
	${SRC}/fpscounter.h
	${SRC}/idgenerator.h
	${SRC}/ringbuffer.h
	${SRC}/speedcounter.h
	${SRC}/utils.h
)

set( grim_tools_SOURCES
#	${SRC}/displaymodeswitcher.cpp
	${SRC}/fpscounter.cpp
	${SRC}/idgenerator.cpp
	${SRC}/ringbuffer.cpp
	${SRC}/speedcounter.cpp
	${SRC}/utils.cpp
)


# fill all grim sources
grim_add_sources( ${grim_tools_SOURCES} )


# Qt3 wrappers
qt4_wrap_cpp( grim_tools_MOC_SOURCES ${grim_tools_HEADERS} OPTIONS "-nw" )


# all sources
set( grim_tools_ALL_SOURCES ${grim_tools_SOURCES} ${grim_tools_MOC_SOURCES} )


add_library( libGrimTools SHARED ${grim_tools_ALL_SOURCES} )
target_link_libraries( libGrimTools ${QT_LIBRARIES} )
if ( UNIX )
	target_link_libraries( libGrimTools X11 Xext Xxf86dga Xxf86vm )
endif ( UNIX )
set_target_properties( libGrimTools PROPERTIES VERSION "${GRIM_VERSION}" SOVERSION "${GRIM_SOVERSION}" OUTPUT_NAME GrimTools )

grim_add_precompiled_headers_wrapper( libGrimTools "${SRC}/precompiledheaders.h" ${grim_tools_ALL_SOURCES} )

grim_get_target_libraries( _interface_library _runtime_library libGrimTools )
set( GRIM_TOOLS_LIBRARY "${_interface_library}" PARENT_SCOPE )


# installation
install( TARGETS libGrimTools
	ARCHIVE DESTINATION "${GRIM_ARCHIVE_DESTINATION}"
	LIBRARY DESTINATION "${GRIM_LIBRARY_DESTINATION}"
	RUNTIME DESTINATION "${GRIM_RUNTIME_DESTINATION}"
	)
