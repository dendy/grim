
cmake_minimum_required( VERSION 2.6 )


project( GrimDemo )


if( NOT CMAKE_BUILD_TYPE )
	set( CMAKE_BUILD_TYPE Release )
endif( NOT CMAKE_BUILD_TYPE )


find_package( Qt4 REQUIRED )
set( QT_USE_QTXML 1 )
include( ${QT_USE_FILE} )


find_package( Grim REQUIRED Archive Audio )


# options
if ( GRIM_INSIDE_BUILD )
	# inherit options from grim library
	set( GRIM_DEMO_TRANSLATIONS ${GRIM_TRANSLATIONS} )
	set( GRIM_DEMO_USE_PRECOMPILED_HEADERS ${GRIM_USE_PRECOMPILED_HEADERS} )
else ( GRIM_TRANSLATIONS )
	set( GRIM_DEMO_TRANSLATIONS "all" CACHE STRING "Space separated list of languages to build translations for" )
	set( GRIM_DEMO_USE_PRECOMPILED_HEADERS ON CACHE STRING "Enable precompiled headers support" )
endif ( GRIM_INSIDE_BUILD )


include_directories( "${CMAKE_CURRENT_BINARY_DIR}" )


set( DEMO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )


set( SRC "${DEMO_ROOT_DIR}/src" )
set( UI "${DEMO_ROOT_DIR}/ui" )


# source files
set( demo_HEADERS
	${SRC}/archivetest.h
	${SRC}/audiowidget.h
	${SRC}/browsermodel.h
	${SRC}/languagemanager.h
	${SRC}/mainwindow.h
	${SRC}/player.h
	${SRC}/playerwidget.h
	${SRC}/pointsmodel.h
	${SRC}/resources.h
)

set( demo_SOURCES
	${SRC}/archivetest.cpp
	${SRC}/audiowidget.cpp
	${SRC}/browsermodel.cpp
	${SRC}/languagemanager.cpp
	${SRC}/main.cpp
	${SRC}/mainwindow.cpp
	${SRC}/player.cpp
	${SRC}/playerwidget.cpp
	${SRC}/pointsmodel.cpp
	${SRC}/resources.cpp
)

# forms
set( demo_FORMS
	${UI}/aboutdialog.ui
	${UI}/archivetest.ui
	${UI}/audiowidget.ui
	${UI}/browserwidget.ui
	${UI}/mainwindow.ui
	${UI}/playerwidget.ui
)

set( demo_RESOURCES
	res/res.qrc
)


# translations
set( _demo_translations )
if ( GRIM_DEMO_TRANSLATIONS )
	separate_arguments( GRIM_DEMO_TRANSLATIONS )
	list( FIND GRIM_DEMO_TRANSLATIONS "all" _all_index )
	if ( NOT _all_index EQUAL -1 )
		grim_locate_translations( _demo_translations "${DEMO_ROOT_DIR}/translations" "info_" )
	else ( NOT _all_index EQUAL -1 )
		set( _demo_translations ${GRIM_DEMO_TRANSLATIONS} )
	endif ( NOT _all_index EQUAL -1 )
endif ( GRIM_DEMO_TRANSLATIONS )

if ( _demo_translations )
	set( _info_ts_sources "${SRC}/languagemanager.cpp" )
	set( _demo_ts_sources ${demo_SOURCES} ${demo_FORMS} )
	list( REMOVE_ITEM _demo_ts_sources ${_info_ts_sources} )
	
	set( demo_QM_FILES )
	set( demo_translation_targets )
	
	# rules for generating .ts from sources and .qm from .ts
	foreach( _language_name ${_demo_translations} )
		set( _info_qm_file )
		set( _demo_qm_file )
	
		set( _info_ts_file "${DEMO_ROOT_DIR}/translations/info_${_language_name}.ts" )
		set( _demo_ts_file "${DEMO_ROOT_DIR}/translations/demo_${_language_name}.ts" )
	
		grim_create_translation( _info_qm_file "" demo_translation_targets ${_info_ts_file} ${_info_ts_sources} OPTIONS -silent )
		grim_create_translation( _demo_qm_file "" demo_translation_targets ${_demo_ts_file} ${_demo_ts_sources} OPTIONS -silent )
	
		list( APPEND demo_QM_FILES ${_info_qm_file} )
		list( APPEND demo_QM_FILES ${_demo_qm_file} )
	
		# append Qt translations into list of .qm files
		if ( QT_TRANSLATIONS_DIR )
			set( _qt_qm_file "${QT_TRANSLATIONS_DIR}/qt_${_language_name}.qm" )
			if ( EXISTS ${_qt_qm_file} )
				list( APPEND demo_QM_FILES ${_qt_qm_file} )
			endif ( EXISTS ${_qt_qm_file} )
		endif ( QT_TRANSLATIONS_DIR )
	endforeach( _language_name )
	
	# rules for link .qm files as resources
	set( _lang_qrc "${CMAKE_CURRENT_BINARY_DIR}/demo_lang.qrc" )
	
	# header
	file( WRITE "${_lang_qrc}"
		"<RCC>\n"
		"  <qresource prefix=\"/\" >\n" )
	# list of qm files
	foreach( _qm_file ${demo_QM_FILES} )
		get_filename_component( _qm_filename "${_qm_file}" NAME )
		file( APPEND "${_lang_qrc}"
			"    <file alias=\"lang/${_qm_filename}\">${_qm_file}</file>\n" )
	endforeach( _qm_file )
	# footer
	file( APPEND "${_lang_qrc}"
		"  </qresource>\n"
		"</RCC>\n" )
	
	# add resource with qm files to list of resources
	qt4_add_resources( demo_RCC_SOURCES "${_lang_qrc}" )
	
	# create custom target to lupdate
	add_custom_target( GrimDemoTranslations DEPENDS ${demo_translation_targets} )
endif ( _demo_translations )




# wrappers
set( demo_ALL_HEADERS ${demo_HEADERS} )

qt4_wrap_cpp( demo_MOC_SOURCES ${demo_ALL_HEADERS} OPTIONS "-nw" )
qt4_wrap_ui( demo_UI_HEADERS ${demo_FORMS} )
qt4_add_resources( demo_RCC_SOURCES ${demo_RESOURCES} )

set( demo_ALL_SOURCES ${demo_SOURCES} ${demo_MOC_SOURCES} ${demo_RCC_SOURCES} )




# target
add_executable( GrimDemo ${demo_QM_FILES} ${demo_UI_HEADERS} ${demo_ALL_SOURCES} )
target_link_libraries( GrimDemo ${QT_LIBRARIES} ${GRIM_LIBRARIES} )
set_target_properties( GrimDemo PROPERTIES OUTPUT_NAME "GrimDemo" PREFIX "" )


if ( WIN32 )
	set_property( TARGET GrimDemo PROPERTY WIN32_EXECUTABLE ON )
	target_link_libraries( GrimDemo ${QT_QTMAIN_LIBRARY} )
endif ( WIN32 )


if ( GRIM_DEMO_USE_PRECOMPILED_HEADERS )
	grim_add_precompiled_headers( GrimDemo src/precompiledheaders.h ${demo_ALL_SOURCES} )
endif ( GRIM_DEMO_USE_PRECOMPILED_HEADERS )
