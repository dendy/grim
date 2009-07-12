
project( libGrimAudioPluginWave )


set( QT_MIN_VERSION "4.5.0" )
find_package( Qt4 REQUIRED )
set( QT_DONT_USE_QTGUI 1 )
include( ${QT_USE_FILE} )


add_definitions( -DQT_STATICPLUGIN )


include_directories( "${GRIM_ROOT_DIR}/include" )


# paths
set( SRC "${GRIM_ROOT_DIR}/src/audio/formats" )


# sources
set( grim_audio_plugin_wave_HEADERS
	${SRC}/audiowaveformatplugin.h
)

set( grim_audio_plugin_wave_SOURCES
	${SRC}/audiowaveformatplugin.cpp
)


# Qt4 wrappers
set( grim_audio_plugin_wave_MOC_SOURCES )
qt4_wrap_cpp( grim_audio_plugin_wave_MOC_SOURCES ${grim_audio_plugin_wave_HEADERS} OPTIONS "-nw" )


# all sources
set( grim_audio_plugin_wave_ALL_SOURCES ${grim_audio_plugin_wave_SOURCES} ${grim_audio_plugin_wave_MOC_SOURCES} )


add_library( GrimAudioPluginWave STATIC ${grim_audio_plugin_wave_ALL_SOURCES} )

if ( UNIX )
	set_target_properties( GrimAudioPluginWave PROPERTIES COMPILE_FLAGS "-fPIC" )
endif ( UNIX )

#grim_add_precompiled_headers( GrimAudioWaveFormatPlugin
#	"${GRIM_ROOT_DIR}/src/audio/precompiledheaders.h" ${grim_audio_plugin_wave_ALL_SOURCES} )


grim_audio_add_format_plugin(
	GrimAudioPluginWave
	grim_audio_wave_format_plugin
)
