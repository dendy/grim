
cmake_minimum_required( VERSION 2.6 )


project( GrimTrivialArchive )


find_package( Qt4 REQUIRED )
include( ${QT_USE_FILE} )

find_package( Grim REQUIRED Archive )


add_executable( GrimTrivialArchive main.cpp )
target_link_libraries( GrimTrivialArchive ${QT_LIBRARIES} ${GRIM_ARCHIVE_LIBRARY} )
set_target_properties( GrimTrivialArchive PROPERTIES OUTPUT_NAME "GrimTrivialArchive" PREFIX "" )
