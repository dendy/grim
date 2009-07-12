
cmake_minimum_required( VERSION 2.6 )


project( GrimTrivialAudio )


find_package( Qt4 REQUIRED )
include( ${QT_USE_FILE} )


find_package( Grim REQUIRED Audio )


add_executable( GrimTrivialAudio main.cpp )
target_link_libraries( GrimTrivialAudio ${QT_LIBRARIES} ${GRIM_AUDIO_LIBRARY} )
set_target_properties( GrimTrivialAudio PROPERTIES OUTPUT_NAME "GrimTrivialAudio" PREFIX "" )
