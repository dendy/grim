/******************************************************************************
 *
 * Grim - Game engine library
 * Copyright (C) 2009 Daniel Levin (dendy.ua@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3.0 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The GNU General Public License is contained in the file LICENSE.GPL3 in the
 * packaging of this file. Please review information in this file to ensure
 * the GNU General Public License version 3.0 requirements will be met.
 *
 * Copy of GNU General Public License available at:
 * http://www.gnu.org/copyleft/gpl.html
 *
 *****************************************************************************/

#include "audiomanager.h"

#include <QPluginLoader>
#include <QStringList>
#include <QCoreApplication>
#include <QDebug>

#include "audio_p.h"
#include "audiodevice.h"
#include "audiocapturedevice.h"
#include "audiocontext.h"
#include "audioformatplugin.h"

// generated automatically by CMake
#include "generated/plugins.h"




namespace Grim {




// check for missed tokens in AL/alc.h
#ifndef ALC_DEFAULT_ALL_DEVICES_SPECIFIER
#  define ALC_DEFAULT_ALL_DEVICES_SPECIFIER        0x1012
#endif

#ifndef ALC_ALL_DEVICES_SPECIFIER
#  define ALC_ALL_DEVICES_SPECIFIER                0x1013
#endif




static QReadWriteLock _sharedManagerMutex( QReadWriteLock::Recursive );
static AudioManager * _sharedManager = 0;




static const char * const AlcEnumerationExt  = "ALC_ENUMERATION_EXT";
static const char * const AlcEnumerateAllExt = "ALC_ENUMERATE_ALL_EXT";
static const char * const AlcExtCapture      = "ALC_EXT_CAPTURE";
static const char * const AlcExtEfx          = "ALC_EXT_EFX";




static const int CaptureDefaultChannelsCount    = 1;
static const int CaptureDefaultBitsPerSample    = 8;
static const int CaptureDefaultFrequency        = 22050;
static const int CaptureDefaultMaxSamples       = 65536;




static ALenum toOpenALFormat( int channels, int bitsPerSample )
{
	if ( channels == 1 )
	{
		if ( bitsPerSample == 8 )
			return AL_FORMAT_MONO8;
		if ( bitsPerSample == 16 )
			return AL_FORMAT_MONO16;
		Q_ASSERT( false );
	}

	if ( channels == 2 )
	{
		if ( bitsPerSample == 8 )
			return AL_FORMAT_STEREO8;
		if ( bitsPerSample == 16 )
			return AL_FORMAT_STEREO16;
		Q_ASSERT( false );
	}

	Q_ASSERT( false );
	return 0;
}




AudioManagerPrivate * AudioManagerPrivate::sharedManagerPrivate()
{
	return AudioManager::sharedManager()->d_;
}


QList<QByteArray> AudioManagerPrivate::parseAlcString( const ALCchar * string )
{
	QList<QByteArray> list;

	if ( string[ 0 ] == 0 )
		return list;

	int pos = 0;
	while ( true )
	{
		if ( string[ pos ] == 0 )
			break;

		QByteArray value = string + pos;
		list << value;

		pos += value.length() + 1;
	}

	return list;
}


AudioManagerPrivate::AudioManagerPrivate( AudioManager * manager ) :
	manager_( manager ),
	currentAudioContextsMutex_( QReadWriteLock::Recursive ),
	audioDevicesMutex_( QReadWriteLock::Recursive )
{
	// load static format plugins
	{
		for ( QListIterator<QObject*> pluginsIt( QPluginLoader::staticInstances() ); pluginsIt.hasNext(); )
		{
			AudioFormatPlugin * plugin = qobject_cast<AudioFormatPlugin*>( pluginsIt.next() );
			if ( !plugin )
				continue;

			_addAudioFormatPlugin( plugin );
		}
	}

	const bool hasEnumerationExt = alcIsExtensionPresent( 0, AlcEnumerationExt );
	if ( hasEnumerationExt )
	{
		// obtain available device names
		availableDeviceNames_ = parseAlcString( alcGetString( 0, ALC_DEVICE_SPECIFIER ) );
		availableCaptureDeviceNames_ = parseAlcString( alcGetString( 0, ALC_CAPTURE_DEVICE_SPECIFIER ) );

		defaultDeviceName_ = alcGetString( 0, ALC_DEFAULT_DEVICE_SPECIFIER );
		defaultCaptureDeviceName_ = alcGetString( 0, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER );
	}

	const bool hasEnumerateAllExt = alcIsExtensionPresent( 0, AlcEnumerateAllExt );
	if ( hasEnumerateAllExt )
	{
		availableAllDeviceNames_ = parseAlcString( alcGetString( 0, ALC_ALL_DEVICES_SPECIFIER ) );
		defaultAllDeviceName_ = alcGetString( 0, ALC_DEFAULT_ALL_DEVICES_SPECIFIER );
	}

	currentAudioContext_ = 0;
}


AudioManagerPrivate::~AudioManagerPrivate()
{
	// ensure all audio devices are closed
	{
		QWriteLocker audioDevicesLocker( &audioDevicesMutex_ );
		Q_ASSERT( audioDevices_.isEmpty() );
	}

	// release plugin loaders
	{
		for ( QListIterator<QPluginLoader*> it( pluginLoaders_ ); it.hasNext(); )
			it.next()->unload();
	}
}


QList<QByteArray> AudioManagerPrivate::availableFileFormats() const
{
	QReadLocker fileFormatsLocker( (QReadWriteLock*)&fileFormatsMutex_ );
	return availableFileFormats_;
}


QList<QString> AudioManagerPrivate::availableFileFormatExtensions() const
{
	QReadLocker fileFormatsLocker( (QReadWriteLock*)&fileFormatsMutex_ );
	return availableFileFormatExtensions_;
}


AudioFormatFile * AudioManagerPrivate::_createFormatFileFromPlugins( const AudioFormatPluginList & plugins,
	const QString & fileName, const QByteArray & format )
{
	QWriteLocker fileFormatsLocker( &fileFormatsMutex_ );

	for ( QListIterator<AudioFormatPlugin*> it( plugins ); it.hasNext(); )
	{
		AudioFormatPlugin * plugin = it.next();
		AudioFormatFile * file = plugin->createFile( fileName, format );
		Q_ASSERT( file );
		if ( file->device()->open( QIODevice::ReadOnly ) )
			return file;
		delete file;
	}

	return 0;
}


AudioFormatFile * AudioManagerPrivate::createFormatFile( const QString & fileName, const QByteArray & format )
{
	// lookup plugin by the given format name
	if ( !format.isNull() )
		return _createFormatFileFromPlugins( audioFormatPluginsForName_.value( format ), fileName, format );

	// lookup plugin by the file name extension
	const int dotPos = fileName.lastIndexOf( QLatin1Char( '.' ) );
	const int slashPos = fileName.lastIndexOf( QLatin1Char( '/' ) );

	AudioFormatPluginList restPlugins = audioFormatPlugins_;
	AudioFormatPluginList extensionPlugins;

	if ( dotPos != -1 && dotPos != fileName.length()-1 && slashPos < dotPos )
	{
		// file name has extension
		const QString extension = fileName.mid( dotPos + 1 ).toLower();
		Q_ASSERT( !extension.isEmpty() );

		extensionPlugins = audioFormatPluginsForExtension_.value( extension );
		AudioFormatFile * file = _createFormatFileFromPlugins( extensionPlugins, fileName, format );
		if ( file )
			return file;
	}

	// file name has no extension, try all plugins on by one, except we checked earlier by extension
	{
		for ( QListIterator<AudioFormatPlugin*> it( extensionPlugins ); it.hasNext(); )
			restPlugins.removeOne( it.next() );
	}

	return _createFormatFileFromPlugins( restPlugins, fileName, format );
}


void AudioManagerPrivate::_addAudioFormatPlugin( AudioFormatPlugin * plugin )
{
	for ( QListIterator<QByteArray> formatIt( plugin->formats() ); formatIt.hasNext(); )
	{
		const QByteArray & format = formatIt.next();

		if ( !availableFileFormats_.contains( format ) )
			availableFileFormats_ << format;

		audioFormatPluginsForName_[ format ] << plugin;
	}

	for ( QStringListIterator extensionIt( plugin->extensions() ); extensionIt.hasNext(); )
	{
		const QString extension = extensionIt.next().toLower();

		if ( !availableFileFormatExtensions_.contains( extension ) )
			availableFileFormatExtensions_ << extension;

		audioFormatPluginsForExtension_[ extension ] << plugin;
	}
}


AudioDevice * AudioManagerPrivate::createDevice( const QByteArray & deviceName )
{
	ALCdevice * alcDevice = alcOpenDevice( deviceName.isEmpty() ? 0 : (ALCchar*)deviceName.constData() );
	if ( !alcDevice )
	{
		qWarning() <<
			"Grim::AudioManagerPrivate::createDevice() : Failed create device with name:" << deviceName;
		return 0;
	}

	AudioDevice * audioDevice = new AudioDevice( new AudioDevicePrivate( deviceName, alcDevice ) );

	{
		QWriteLocker audioDevicesLocker( &audioDevicesMutex_ );
		audioDevices_ << audioDevice;
	}

	return audioDevice;
}


void AudioManagerPrivate::removeDevice( AudioDevice * audioDevice )
{
	QWriteLocker audioDevicesLocker( &audioDevicesMutex_ );
	audioDevices_.removeOne( audioDevice );
}


AudioCaptureDevice * AudioManagerPrivate::createCaptureDevice( const QByteArray & captureDeviceName,
	int channelsCount, int bitsPerSample, int frequency, int maxSamples )
{
	// only mono and stereo allowed in OpenAL 1.1
	if ( channelsCount != 0 && channelsCount != 1 && channelsCount != 2 )
	{
		qWarning( "Grim::AudioManagerPrivate::createCaptureDevice() : "
			"Channels count should equal 1 or 2 for mono and stereo respectively." );
		return 0;
	}

	// only 8 and 16 bits per sample allowed in OpenAL 1.1
	if ( bitsPerSample != 0 && bitsPerSample != 8 && bitsPerSample != 16 )
	{
		qWarning( "Grim::AudioManagerPrivate::createCaptureDevice() : "
			"Bits per sample must be 8 or 16." );
		return 0;
	}

	if ( frequency != 0 && frequency < 0 )
	{
		qWarning( "Grim::AudioManagerPrivate::createCaptureDevice() : "
			"Frequency must be positive." );
		return 0;
	}

	if ( maxSamples != 0 && maxSamples < 0 )
	{
		qWarning( "Grim::AudioManagerPrivate::createCaptureDevice() : "
			"maxSamples must be positive." );
		return 0;
	}

	if ( channelsCount == 0 )
		channelsCount = CaptureDefaultChannelsCount;

	if ( bitsPerSample == 0 )
		bitsPerSample = CaptureDefaultBitsPerSample;

	if ( frequency == 0 )
		frequency = CaptureDefaultFrequency;

	if ( maxSamples == 0 )
		maxSamples = CaptureDefaultMaxSamples;

	ALenum alFormat = toOpenALFormat( channelsCount, bitsPerSample );

	ALCdevice * alcCaptureDevice = alcCaptureOpenDevice( captureDeviceName.isEmpty() ? 0 : (ALCchar*)captureDeviceName.constData(),
		frequency, alFormat, maxSamples );
	if ( !alcCaptureDevice )
	{
		qWarning() <<
			"Grim::AudioManagerPrivate::createCaptureDevice() : Failed create capture device with name:" << captureDeviceName;
		return 0;
	}

	AudioCaptureDevice * audioCaptureDevice = new AudioCaptureDevice(
		new AudioCaptureDevicePrivate( captureDeviceName, alcCaptureDevice, alFormat,
			channelsCount, frequency, bitsPerSample, maxSamples ) );

	{
		QWriteLocker audioCaptureDevicesLocker( &audioCaptureDevicesMutex_ );
		audioCaptureDevices_ << audioCaptureDevice;
	}

	return audioCaptureDevice;
}


void AudioManagerPrivate::removeCaptureDevice( AudioCaptureDevice * audioCaptureDevice )
{
	QWriteLocker audioCaptureDevicesLocker( &audioCaptureDevicesMutex_ );
	audioCaptureDevices_.removeOne( audioCaptureDevice );
}


AudioContextLocker AudioManagerPrivate::lockAudioContext( AudioContext * audioContext )
{
	AudioContextLocker locker;
	locker.d = new AudioContextLockerData( &currentAudioContextsMutex_ );

	if ( currentAudioContext_ == audioContext )
		return locker;

	currentAudioContext_ = audioContext;
	alcMakeContextCurrent( currentAudioContext_->d_->alcContext_ );

	return locker;
}


AudioContextLocker AudioManagerPrivate::lockAudioContextForDevice( AudioDevice * audioDevice )
{
	AudioContextLocker locker;
	locker.d = new AudioContextLockerData( &currentAudioContextsMutex_ );

	QReadLocker audioContextsLocker( &audioDevice->d_->audioContextsMutex_ );
	Q_ASSERT( !audioDevice->d_->audioContexts_.isEmpty() );

	for ( QListIterator<AudioContext*> it( audioDevice->d_->audioContexts_ ); it.hasNext(); )
		if ( it.next() == currentAudioContext_ )
			return locker;

	currentAudioContext_ = audioDevice->d_->audioContexts_.first();
	alcMakeContextCurrent( currentAudioContext_->d_->alcContext_ );

	return locker;
}


void AudioManagerPrivate::unsetCurrentContextIfCurrent( AudioContext * audioContext )
{
	QWriteLocker locker( &currentAudioContextsMutex_ );

	if ( currentAudioContext_ != audioContext )
		return;

	currentAudioContext_ = 0;
	alcMakeContextCurrent( 0 );
}


bool AudioManagerPrivate::verifyOpenALBuffer( const AudioBufferContent & content ) const
{
	// only mono and stereo are allowed
	if ( content.channels != 1 && content.channels != 2 )
		return false;

	// only 8 and 16 bits per sample are allowed
	if ( content.bitsPerSample != 8 && content.bitsPerSample != 16 )
		return false;

	// frequency must be set
	if ( content.frequency <= 0 )
		return false;

	if ( content.isSequential )
	{
		// for sequential devices both positive and unknown sizes are allowed
		if ( content.totalSamples != -1 && content.totalSamples < 0 )
			return false;
	}
	else
	{
		// for random access devices only positive size is allowed
		if ( content.totalSamples < 0 )
			return false;
	}

	return true;
}


AudioOpenALBuffer AudioManagerPrivate::createOpenALBuffer( const AudioBufferContent & content ) const
{
	Q_ASSERT( verifyOpenALBuffer( content ) );
	Q_ASSERT( content.totalSamples >= 0 || (content.totalSamples == -1 && content.isSequential) );

	ALenum alFormat = toOpenALFormat( content.channels, content.bitsPerSample );

	AudioOpenALBuffer alBuffer;

	alBuffer.channels = content.channels;
	alBuffer.bitsPerSample = content.bitsPerSample;
	alBuffer.frequency = content.frequency;
	alBuffer.size = content.data.size();
	alBuffer.samples = content.samples;
	alBuffer.isSequential = content.isSequential;
	alBuffer.totalSamples = content.totalSamples;
	alBuffer.samplesOffset = content.samplesOffset;

	if ( !content.data.isEmpty() )
	{
		alGenBuffers( 1, &alBuffer.id );
		Q_ASSERT( alBuffer.id != 0 );

		alGetError();
		alBufferData( alBuffer.id, alFormat, (ALvoid*)content.data.constData(), content.data.size(), content.frequency );

		if ( alGetError() == AL_OUT_OF_MEMORY )
		{
			qWarning( "Grim::AudioManagerPrivate::createOpenALBuffer() : Failed create OpenAL buffer: out of memory." );
		}
	}

	return alBuffer;
}


void AudioManagerPrivate::destroyOpenALBuffer( const AudioOpenALBuffer & alBuffer ) const
{
	if ( alBuffer.isNull() )
		return;

	alDeleteBuffers( 1, &alBuffer.id );
}




void AudioManagerPrivate::destroySharedManager()
{
	QWriteLocker locker( &_sharedManagerMutex );
	delete _sharedManager;
	_sharedManager = 0;
}


/**
 * Returns audio manager singleton.
 */

AudioManager * AudioManager::sharedManager()
{
	QWriteLocker locker( &_sharedManagerMutex );

	if ( !_sharedManager )
	{
		_sharedManager = new AudioManager;
		qAddPostRoutine( AudioManagerPrivate::destroySharedManager );
	}

	return _sharedManager;
}




/**
 * \class AudioManager
 *
 * \ingroup audio_module
 *
 * \brief The AudioManager class is a singleton for global audio properties and devices construction.
 *
 * \threadsafe
 *
 * Audio manager maintains available audio codecs, audio output devices and audio capture devices.
 *
 * Use createDevice() to construct audio output device for one of available device names or
 * createCaptureDevice() to construct audio capture device.
 */


/** \internal
 */

AudioManager::AudioManager() :
	d_( new AudioManagerPrivate( this ) )
{
}


/** \internal
 */

AudioManager::~AudioManager()
{
	delete d_;
}


/**
 * Returns list of available audio formats.
 */

QList<QByteArray> AudioManager::availableFileFormats() const
{
	return d_->availableFileFormats();
}


/**
 * Returns list of file extensions for available audio formats.
 * For example "ogg" and "oga" for Ogg/Vorbis and "flac" for FLAC.
 * This extensions are used for hint only and speedup internal algorithm for codec selection,
 * when codec format was not specified explicitly.
 */

QList<QString> AudioManager::availableFileFormatExtensions() const
{
	return d_->availableFileFormatExtensions();
}


/**
 * Returns list of available audio output devices names.
 * This list is useful to display it to the end user, so he can select suitable one.
 * Use names from this list to pass them to createDevice().
 *
 * \sa defaultDeviceName(), createDevice()
 */

QList<QByteArray> AudioManager::availableDeviceNames() const
{
	return d_->availableDeviceNames_;
}


/**
 * Returns default audio output device name, that will be used if passing name to
 * createDevice() was omitted.
 *
 * \sa availableDeviceNames(), createDevice()
 */

QByteArray AudioManager::defaultDeviceName() const
{
	return d_->defaultDeviceName_;
}


/**
 * Returns list of available audio capture devices names.
 * This list is useful to display it to the end user, so he can select suitable one.
 * Use names from this list to pass them to createCaptureDevice().
 *
 * \sa defaultCaptureDeviceName(), createCaptureDevice()
 */

QList<QByteArray> AudioManager::availableCaptureDeviceNames() const
{
	return d_->availableCaptureDeviceNames_;
}


/**
 * Returns default audio capture device name, that will be used if passing name to
 * createCaptureDevice() was omitted.
 *
 * \sa availableCaptureDeviceNames(), createCaptureDevice()
 */

QByteArray AudioManager::defaultCaptureDeviceName() const
{
	return d_->defaultCaptureDeviceName_;
}


/**
 * This method is similar to availableDeviceNames(), but returns even more possible device names
 * found in system.
 * Use this list to display audio devices to advanced users.
 *
 * \sa availableDeviceNames(), createDevice()
 */

QList<QByteArray> AudioManager::availableAllDeviceNames() const
{
	return d_->availableAllDeviceNames_;
}


/**
 * Returns default audio output device name from the availableAllDeviceNames() list.
 *
 * \sa availableAllDeviceNames(), createDevice()
 */

QByteArray AudioManager::defaultAllDeviceName() const
{
	return d_->defaultAllDeviceName_;
}


/**
 * Constructs connection to the audio output device with the given \a deviceName and returns
 * audio device instance that holds it.
 * If \a deviceName was omitted then defaultDeviceName() will be used.
 * Returns \c 0 if constructing device instance was failed.
 *
 * \sa createCaptureDevice()
 */

AudioDevice * AudioManager::createDevice( const QByteArray & deviceName )
{
	return d_->createDevice( deviceName );
}


/**
 * Constructs connection to the audio capture device with the given \a captureDeviceName and returns
 * audio device instance that holds it.
 * If \a captureDeviceName was omitted then defaultCaptureDeviceName() will be used.
 *
 * Specifying capture device options is optional, use them to control format of the captured samples.
 * Omit any of them to use default ones.
 *
 * \a channelsCount specifies number of channels to capture, possible values are \c 1 for mono and \c 2 for stereo.
 *
 * \a bitsPerSample specifies number of bits per sample, possible values are \c 8 and \c 16 bits per sample.
 *
 * \a frequency specifies number of captured samples per second, it must be positive value in Hz.
 *
 * Because of capturing occurs in real time internal buffer with samples can be overfilled with samples
 * if you'll forget to take them.
 * Normally samples should be taken and processed as fast as possible, for example to stream voice to network in
 * real time, so maximum internal buffers size should not be very big.
 * Pass \a maxSamples to control the size of internal samples buffer.
 *
 * If capture device failed to create \0 is returned.
 *
 * \sa createDevice()
 */

AudioCaptureDevice * AudioManager::createCaptureDevice( const QByteArray & captureDeviceName,
	int channelsCount, int bitsPerSample, int frequency, int maxSamples )
{
	return d_->createCaptureDevice( captureDeviceName, channelsCount, bitsPerSample, frequency, maxSamples );
}




} // namespace Grim
