
#include <QApplication>
#include <QLabel>

#include <grim/audio/audiomanager.h>
#include <grim/audio/audiodevice.h>
#include <grim/audio/audiocontext.h>
#include <grim/audio/audiosource.h>
#include <grim/audio/audiobuffer.h>

#include <stdio.h>




static const QByteArray AlsaDeviceName = "ALSA Software";




int usage()
{
	printf(
		"Usage:\n"
		"  TrivialAudio <path to audio file> [format]\n"
		"\n"
		"List of available audio formats:\n"
		);

	for ( QListIterator<QByteArray> it( Grim::AudioManager::sharedManager()->availableFileFormats() ); it.hasNext(); )
		printf( "  %s\n", it.next().constData() );

	printf( "\n" );

	return 0;
}


int no_device( const QByteArray & deviceName )
{
	printf( "Failed to create audio device with name: %s\n", deviceName.constData() );
	return 1;
}


int no_context()
{
	printf( "Failed to create audio context\n" );
	return 1;
}


int main( int argc, char ** argv )
{
	QApplication app( argc, argv );

	if ( argc < 2 )
		return usage();

	const QByteArray deviceName = Grim::AudioManager::sharedManager()->availableDeviceNames().contains( AlsaDeviceName ) ?
		AlsaDeviceName : Grim::AudioManager::sharedManager()->defaultDeviceName();

	Grim::AudioDevice * audioDevice = Grim::AudioManager::sharedManager()->createDevice( deviceName );
	if ( !audioDevice )
		return no_device( deviceName );

	Grim::AudioContext * audioContext = audioDevice->createContext();
	if ( !audioContext )
	{
		delete audioDevice;
		return no_context();
	}

	Grim::AudioBuffer audioBuffer = audioContext->createBuffer(
		argv[1],                                         // path to file
		argc > 2 ? QByteArray( argv[2] ) : QByteArray(), // file format
		Grim::AudioBuffer::Streaming                     // buffer policy
		);

	Grim::AudioSource * audioSource = audioContext->createSource();
	audioSource->setBuffer( audioBuffer );
	audioBuffer = Grim::AudioBuffer();

	audioSource->play();

	QLabel label( "Close me to stop playing" );
	label.show();

	app.exec();

	delete audioContext;
	delete audioDevice;

	return 0;
}
