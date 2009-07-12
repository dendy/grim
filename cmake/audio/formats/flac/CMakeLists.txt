
project( libGrimAudioPluginFlac )


set( QT_MIN_VERSION "4.5.0" )
find_package( Qt4 REQUIRED )
set( QT_DONT_USE_QTGUI 1 )
include( ${QT_USE_FILE} )

# Xiph libraries: Ogg & Vorbis
find_package( FLAC )


if ( NOT FLAC_FOUND )
	message( STATUS
		"\n"
		"FLAC libraries are not found.\n"
		"Audio FLAC plugin will be skipped from build.\n"
		"Resolve prerequisites for FLAC libraries and rerun configuration."
		"\n" )
	return()
endif ( NOT FLAC_FOUND )



add_definitions( -DQT_STATICPLUGIN )


include_directories( "${GRIM_ROOT_DIR}/include" )


# paths
set( SRC "${GRIM_ROOT_DIR}/src/audio/formats" )


# sources
set( grim_audio_plugin_flac_HEADERS
	${SRC}/audioflacformatplugin.h
)

set( grim_audio_plugin_flac_SOURCES
	${SRC}/audioflacformatplugin.cpp
)


# Qt4 wrappers
set( grim_audio_plugin_flac_MOC_SOURCES )
qt4_wrap_cpp( grim_audio_plugin_flac_MOC_SOURCES ${grim_audio_plugin_flac_HEADERS} OPTIONS "-nw" )


# all sources
set( grim_audio_plugin_flac_ALL_SOURCES ${grim_audio_plugin_flac_SOURCES} ${grim_audio_plugin_flac_MOC_SOURCES} )


add_library( GrimAudioPluginFlac STATIC ${grim_audio_plugin_flac_ALL_SOURCES} )

set_property( TARGET GrimAudioPluginFlac PROPERTY LINK_INTERFACE_LIBRARIES ${FLAC_LIBRARY} )

if ( UNIX )
	set_target_properties( GrimAudioPluginFlac PROPERTIES COMPILE_FLAGS "-fPIC" )
endif ( UNIX )

#grim_add_precompiled_headers( GrimAudioPluginFlac
#	"${GRIM_ROOT_DIR}/src/audio/precompiledheaders.h" ${grim_audio_plugin_flac_ALL_SOURCES} )


grim_audio_add_format_plugin(
	GrimAudioPluginFlac
	grim_audio_flac_format_plugin
)
