
#include <QApplication>
#include <QDirIterator>

#include <grim/archive/archive.h>

#include <stdio.h>




int usage()
{
	printf(
		"Usage:\n"
		"  TrivialArchive <path to ZIP archive>\n\n"
		);

	return 0;
}


int cant_open( const QString & fileName )
{
	printf( "Cannot open archive: %s\n", qPrintable( fileName ) );
	return 1;
}


int not_a_zip( const QString & fileName )
{
	printf( "Looks like file is not a ZIP archive: %s\n", qPrintable( fileName ) );
	return 1;
}


int main( int argc, char ** argv )
{
	QApplication app( argc, argv );

	if ( argc < 2 )
		return usage();

	const QString fileName = QString::fromLocal8Bit( argv[1] );
	Grim::Archive archive( fileName );

	if ( !archive.open( Grim::Archive::ReadOnly | Grim::Archive::Block ) )
		return cant_open( fileName );

	if ( archive.isBroken() )
		return not_a_zip( fileName );

	printf( "Listing files in archive...\n\n" );

	for ( QDirIterator it( fileName, QDirIterator::Subdirectories ); it.hasNext(); )
	{
		it.next();
		printf( "%s\n", qPrintable( it.filePath() ) );
	}

	return 0;
}
