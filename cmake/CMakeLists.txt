
cmake_minimum_required( VERSION 2.6 )


project( Grim )


set( GRIM_VERSION "0.0.1" )
set( GRIM_SOVERSION "0.0" )


# additional directory with modules
set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules" )


if( NOT CMAKE_BUILD_TYPE )
	set( CMAKE_BUILD_TYPE Release )
endif( NOT CMAKE_BUILD_TYPE )


# additional macroses
include( Macros )


# global definitions
if ( WIN32 )
	add_definitions( -DUNICODE )
endif ( WIN32 )


set( BUILD_SHARED_LIBS 1 )

set( CMAKE_STATIC_LIBRARY_PREFIX "lib" )
set( CMAKE_SHARED_LIBRARY_PREFIX "lib" )

if ( NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin" )
endif ( NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY )
if ( NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib" )
endif ( NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY )
if ( NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib" )
endif ( NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY )


# global options
option( GRIM_USE_PRECOMPILED_HEADERS "Enable Precompiled Headers support" ON )

# options to build particular grim module
option( GRIM_BUILD_MODULE_GAME    "Build libGrimGame"    OFF )
option( GRIM_BUILD_MODULE_ARCHIVE "Build libGrimArchive" ON )
option( GRIM_BUILD_MODULE_AUDIO   "Build libGrimAudio"   ON )
option( GRIM_BUILD_MODULE_TOOLS   "Build libGrimTools"   ON )

# options to build tests
option( GRIM_BUILD_DEMO "Build demo application" ON )
option( GRIM_BUILD_EXAMPLES "Build examples" ON )

# translation options
set( GRIM_TRANSLATIONS "all" CACHE STRING "Space separated list of languages to build translations for" )


# helper macro for adding precompiled headers
macro ( grim_add_precompiled_headers_wrapper TARGET PRECOMPILED_HEADER )
	if ( GRIM_USE_PRECOMPILED_HEADERS )
		grim_add_precompiled_headers( ${TARGET} ${PRECOMPILED_HEADER} ${ARGN} )
	endif ( GRIM_USE_PRECOMPILED_HEADERS )
endmacro ( grim_add_precompiled_headers_wrapper )


# set of variables for Grim:
#   GRIM_ROOT_DIR
#   GRIM_INCLUDE_DIR
#   GRIM_GAME_LIBRARY
#   GRIM_ARCHIVE_LIBRARY
#   GRIM_AUDIO_LIBRARY
#   GRIM_TOOLS_LIBRARY
#   GRIM_LIBRARIES

get_filename_component( GRIM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE )
set( GRIM_ROOT_DIR "${GRIM_ROOT_DIR}" CACHE PATH "Root directory of Grim library" )
set( GRIM_INCLUDE_DIR "${GRIM_ROOT_DIR}/include" CACHE PATH "Path to Grim include directory" FORCE )
set( GRIM_GAME_LIBRARY    "" CACHE FILEPATH "Path to GrimGame library"    FORCE )
set( GRIM_ARCHIVE_LIBRARY "" CACHE FILEPATH "Path to GrimArchive library" FORCE )
set( GRIM_AUDIO_LIBRARY   "" CACHE FILEPATH "Path to GrimAudio library"   FORCE )
set( GRIM_TOOLS_LIBRARY   "" CACHE FILEPATH "Path to GrimTools library"   FORCE )
set( GRIM_LIBRARIES )


# add include directory for cross-references between modules
include_directories( "${GRIM_INCLUDE_DIR}" )


# check if build is for 64-bit system and set library suffix appropriate
set( GRIM_ARCHIVE_DESTINATION "lib" )
set( GRIM_RUNTIME_DESTINATION "bin" )
set( GRIM_LIBRARY_DESTINATION "lib" )
if ( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" )
	set( GRIM_LIBRARY_DESTINATION "lib64" )
endif ( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" )


# mark global variable that we are inside Grim library build
set( GRIM_INSIDE_BUILD ON )


# macro to collect all grim sources from modules to create solid translation file later
set( grim_ALL_SOURCES )
macro( grim_add_sources )
	set( grim_ALL_SOURCES ${grim_ALL_SOURCES} ${ARGN} PARENT_SCOPE )
endmacro( grim_add_sources )


# list of Grim modules
set( _grim_modules
	"Game"
	"Archive"
	"Audio"
	"Tools"
	)


# game module
if ( GRIM_BUILD_MODULE_GAME )
	add_subdirectory( game )
endif ( GRIM_BUILD_MODULE_GAME )


# archive module
if ( GRIM_BUILD_MODULE_ARCHIVE )
	add_subdirectory( archive )
endif ( GRIM_BUILD_MODULE_ARCHIVE )


# audio module
if ( GRIM_BUILD_MODULE_AUDIO )
	add_subdirectory( audio )
endif ( GRIM_BUILD_MODULE_AUDIO )


# tools module
if ( GRIM_BUILD_MODULE_TOOLS )
	add_subdirectory( tools )
endif ( GRIM_BUILD_MODULE_TOOLS )


# setup collected Grim libraries
# and push all Grim variables upper to parent scope
# so Grim developers can skip find_package( Grim ) step
# in theirs test projects
set( GRIM_LIBRARIES )

foreach( _grim_module ${_grim_modules} )
	string( TOUPPER "${_grim_module}" _grim_module_upper )
	set( _library_variable "GRIM_${_grim_module_upper}_LIBRARY" )

	if ( ${_library_variable} )
		list( APPEND GRIM_LIBRARIES ${${_library_variable}} )
		set( ${_library_variable} "${${_library_variable}}" PARENT_SCOPE )
	endif ( ${_library_variable} )
endforeach( _grim_module ${_grim_modules} )

set( GRIM_ROOT_DIR "${GRIM_ROOT_DIR}" PARENT_SCOPE )
set( GRIM_INCLUDE_DIR "${GRIM_INCLUDE_DIR}" PARENT_SCOPE )
set( GRIM_LIBRARIES "${GRIM_LIBRARIES}" PARENT_SCOPE )


# demo application
if ( GRIM_BUILD_DEMO )
	if ( GRIM_BUILD_MODULE_ARCHIVE AND GRIM_BUILD_MODULE_AUDIO AND GRIM_BUILD_MODULE_TOOLS )
		add_subdirectory( "${GRIM_ROOT_DIR}/demo" "demo" )
		add_dependencies( GrimDemo libGrimArchive libGrimAudio libGrimTools )
	endif ( GRIM_BUILD_MODULE_ARCHIVE AND GRIM_BUILD_MODULE_AUDIO AND GRIM_BUILD_MODULE_TOOLS )
endif ( GRIM_BUILD_DEMO )


# examples
if ( GRIM_BUILD_EXAMPLES )
	get_filename_component( GRIM_EXAMPLES_DIR "${GRIM_ROOT_DIR}/examples" ABSOLUTE )

	if ( GRIM_BUILD_MODULE_ARCHIVE )
		add_subdirectory( "${GRIM_EXAMPLES_DIR}/trivialarchive" "examples/trivialarchive" )
		add_dependencies( GrimTrivialArchive libGrimArchive )
	endif ( GRIM_BUILD_MODULE_ARCHIVE )

	if ( GRIM_BUILD_MODULE_AUDIO )
		add_subdirectory( "${GRIM_EXAMPLES_DIR}/trivialaudio" "examples/trivialaudio" )
		add_dependencies( GrimTrivialAudio libGrimAudio )
	endif ( GRIM_BUILD_MODULE_AUDIO )
endif ( GRIM_BUILD_EXAMPLES )


# create translations for all grim libraries
separate_arguments( GRIM_TRANSLATIONS )
list( FIND GRIM_TRANSLATIONS "all" _all_index )
if ( NOT _all_index EQUAL -1 )
	grim_locate_translations( _grim_translations "${GRIM_ROOT_DIR}/translations" "grim_" )
else ( NOT _all_index EQUAL -1 )
	set( _grim_translations ${GRIM_TRANSLATIONS} )
endif ( NOT _all_index EQUAL -1 )

set( grim_translation_targets )
set( grim_qm_files )
foreach ( _language_name ${_grim_translations} )
	set( _grim_ts_file "${GRIM_ROOT_DIR}/translations/grim_${_language_name}.ts" )
	grim_create_translation( _qm_file "" grim_translation_targets ${_grim_ts_file} ${grim_ALL_SOURCES} OPTIONS -silent )
	list( APPEND grim_qm_files ${_qm_file} )
endforeach ( _language_name ${_grim_translations} )

# create custom target to lupdate
add_custom_target( GrimTranslations DEPENDS ${grim_translation_targets} )


# installation

# resolve include files references and install real headers from src/ directory
# instead of dummy wrappers from include/ directory
file( GLOB_RECURSE _include_wrappers RELATIVE "${GRIM_INCLUDE_DIR}" "${GRIM_INCLUDE_DIR}/*.h" )
foreach( _wrapper ${_include_wrappers} )
	get_filename_component( _wrapper_path "${_wrapper}" PATH )

	file( READ "${GRIM_INCLUDE_DIR}/${_wrapper}" _wrapper_contents )
	string( REPLACE "#include" "" _wrapper_contents "${_wrapper_contents}" )
	string( REPLACE "\"" "" _wrapper_contents "${_wrapper_contents}" )
	string( STRIP "${_wrapper_contents}" _target )

	install( FILES "${GRIM_INCLUDE_DIR}/${_wrapper_path}/${_target}" DESTINATION "include/${_wrapper_path}" )
endforeach( _wrapper ${_include_wrappers} )

# documentation
set( _doc_html_dir "${GRIM_ROOT_DIR}/doc/html" )
if ( EXISTS "${_doc_html_dir}" )
	install( DIRECTORY "${_doc_html_dir}" DESTINATION "share/doc/grim-${GRIM_VERSION}" )
endif ( EXISTS "${_doc_html_dir}" )

# FindGrim.cmake
install( FILES "${GRIM_ROOT_DIR}/cmake/modules/FindGrim.cmake" DESTINATION "share/cmake/Modules" )

# Macros.cmake
install( FILES "${GRIM_ROOT_DIR}/cmake/modules/Macros.cmake" DESTINATION "share/grim/cmake/modules" )
