
project( libGrimAudioPluginVorbis )


set( QT_MIN_VERSION "4.5.0" )
find_package( Qt4 REQUIRED )
set( QT_DONT_USE_QTGUI 1 )
include( ${QT_USE_FILE} )

# Xiph libraries: Ogg & Vorbis
find_package( Ogg )
find_package( Vorbis )


if ( NOT Ogg_FOUND OR NOT Vorbis_FOUND )
	message( STATUS
		"\n"
		"Ogg/Vorbis libraries are not found.\n"
		"Audio Vorbis plugin will be skipped from build.\n"
		"Resolve prerequisites for Ogg/Vorbis libraries and rerun configuration."
		"\n" )
	return()
endif ( NOT Ogg_FOUND OR NOT Vorbis_FOUND )


add_definitions( -DQT_STATICPLUGIN )


include_directories( "${GRIM_ROOT_DIR}/include" )


# paths
set( SRC "${GRIM_ROOT_DIR}/src/audio/formats" )


# sources
set( grim_audio_plugin_vorbis_HEADERS
	${SRC}/audiovorbisformatplugin.h
)

set( grim_audio_plugin_vorbis_SOURCES
	${SRC}/audiovorbisformatplugin.cpp
)


# Qt4 wrappers
set( grim_audio_plugin_vorbis_MOC_SOURCES )
qt4_wrap_cpp( grim_audio_plugin_vorbis_MOC_SOURCES ${grim_audio_plugin_vorbis_HEADERS} OPTIONS "-nw" )


# all sources
set( grim_audio_plugin_vorbis_ALL_SOURCES ${grim_audio_plugin_vorbis_SOURCES} ${grim_audio_plugin_vorbis_MOC_SOURCES} )


add_library( GrimAudioPluginVorbis STATIC ${grim_audio_plugin_vorbis_ALL_SOURCES} )

set_property( TARGET GrimAudioPluginVorbis PROPERTY LINK_INTERFACE_LIBRARIES ${OGG_LIBRARY} ${VORBIS_LIBRARY} ${VORBISFILE_LIBRARY} )

if ( UNIX )
	set_target_properties( GrimAudioPluginVorbis PROPERTIES COMPILE_FLAGS "-fPIC" )
endif ( UNIX )

#grim_add_precompiled_headers( GrimAudioPluginVorbis
#	"${GRIM_ROOT_DIR}/src/audio/precompiledheaders.h" ${grim_audio_plugin_vorbis_ALL_SOURCES} )


grim_audio_add_format_plugin(
	GrimAudioPluginVorbis
	grim_audio_vorbis_format_plugin
)
