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
 * The GNU General Public License is contained in the file COPYING in the
 * packaging of this file. Please review information in this file to ensure
 * the GNU General Public License version 3.0 requirements will be met.
 *
 * Copy of GNU General Public License available at:
 * http://www.gnu.org/copyleft/gpl.html
 *
 *****************************************************************************/

#include "audiodevice.h"

#include <QDebug>

#include "audio_p.h"
#include "audiomanager.h"
#include "audiocontext.h"
#include "audiobufferloader.h"
#include "audiobufferdata.h"




namespace Grim {




/** \internal
 * \class AudioDevicePrivate
 */


AudioDevicePrivate::AudioDevicePrivate( const QByteArray & name, ALCdevice * alcDevice ) :
	name_( name ), alcDevice_( alcDevice )
{
	extensionNames_ = AudioManagerPrivate::parseAlcString( alcGetString( alcDevice_, ALC_EXTENSIONS ) );

	// obtaining version and attributes
	alcGetIntegerv( alcDevice_, ALC_MAJOR_VERSION, 1, &versionMajor_ );
	alcGetIntegerv( alcDevice_, ALC_MAJOR_VERSION, 1, &versionMinor_ );

	bufferLoader_ = new AudioBufferLoader( this );
	connect( bufferLoader_, SIGNAL(requestFinished(int,void*)),
		SLOT(_requestFinished(int,void*)), Qt::DirectConnection );
}


AudioDevicePrivate::~AudioDevicePrivate()
{
	// destroy buffer loader
	{
		delete bufferLoader_;
		bufferLoader_ = 0;

		QWriteLocker audioBufferRequestsLocker( &audioBufferRequestsMutex_ );
		requestItemForId_.clear();
	}

	// ensure audio contexts are removed
	{
		QWriteLocker audioContextsLocker( &audioContextsMutex_ );
		Q_ASSERT_X( audioContexts_.isEmpty(),
			"Grim::AudioDevicePrivate::~AudioDevicePrivate()",
			"All audio contexts must be destroyed explicitly." );
	}

	// ensure audio buffers destroyed
	{
		QWriteLocker audioBuffersLocker( &audioBuffersMutex_ );
		Q_ASSERT_X( audioBuffers_.isEmpty(),
			"Grim::AudioDevicePrivate::~AudioDevicePrivate()",
			"All audio buffers must be destroyed explicitly." );
	}

	alcCloseDevice( alcDevice_ );
	alcDevice_ = 0;

	// remove self from manager
	AudioManagerPrivate::sharedManagerPrivate()->removeDevice( audioDevice_ );
}


/**
 * Locks the first found audio context and returns entity that prevents
 * OpenAL context switching until will not be destructed.
 */

AudioContextLocker AudioDevicePrivate::lock()
{
	return AudioManagerPrivate::sharedManagerPrivate()->lockAudioContextForDevice( audioDevice_ );
}


AudioContext * AudioDevicePrivate::createContext( int frequency, int refreshInterval, int sync, int monoSources, int stereoSources )
{
	ALCint array[ 11 ]; // 11 is for 5 tokens + 5 values + 0-terminator
	int i = 0;

	if ( frequency != -1 )
	{
		array[ i++ ] = ALC_FREQUENCY;
		array[ i++ ] = frequency;
	}

	if ( refreshInterval != -1 )
	{
		array[ i++ ] = ALC_REFRESH;
		array[ i++ ] = refreshInterval;
	}

	if ( sync != -1 )
	{
		array[ i++ ] = ALC_SYNC;
		array[ i++ ] = sync ? ALC_TRUE : ALC_FALSE;
	}

	if ( monoSources != -1 )
	{
		array[ i++ ] = ALC_MONO_SOURCES;
		array[ i++ ] = monoSources;
	}

	if ( stereoSources != -1 )
	{
		array[ i++ ] = ALC_STEREO_SOURCES;
		array[ i++ ] = stereoSources;
	}

	array[ i ] = 0;

	ALCcontext * alcContext = alcCreateContext( alcDevice_, i == 0 ? 0 : array );
	if ( !alcContext )
	{
		qWarning( "Grim::AudioDevicePrivate::createContext() : Failed to create audio context." );
		return 0;
	}

	AudioContext * audioContext = new AudioContext( new AudioContextPrivate( audioDevice_, alcContext ) );

	{
		QWriteLocker audioContextsLocker( &audioContextsMutex_ );
		audioContexts_ << audioContext;
	}

	return audioContext;
}


void AudioDevicePrivate::removeContext( AudioContext * audioContext )
{
	QWriteLocker audioContextLocker( &audioContextsMutex_ );
	audioContexts_.removeOne( audioContext );
}


AudioBuffer AudioDevicePrivate::createBuffer( const QString & fileName, const QByteArray & format, AudioBuffer::Policy policy )
{
	AudioBuffer audioBuffer( new AudioBufferPrivate( audioDevice_, fileName, format, policy ) );

	{
		QWriteLocker audioBuffersLocker( &audioBuffersMutex_ );
		audioBuffers_ << audioBuffer;
	}

	return audioBuffer;
}


AudioBuffer AudioDevicePrivate::createBuffer( const AudioBufferData & audioBufferData, AudioContext * audioContext )
{
	if ( audioBufferData.isNull() )
		return AudioBuffer();

	AudioBufferContent content;
	content.channels = audioBufferData.channelsCount();
	content.bitsPerSample = audioBufferData.bitsPerSample();
	content.frequency = audioBufferData.frequency();
	content.isSequential = false;
	content.totalSamples = audioBufferData.samplesCount();
	content.samplesOffset = 0;
	content.samples = content.totalSamples;
	content.data = audioBufferData.data();

	AudioOpenALBuffer alBuffer;
	{
		AudioContextLocker contextLocker = audioContext->d_->lock();
		alBuffer = AudioManagerPrivate::sharedManagerPrivate()->createOpenALBuffer( content );
	}

	AudioBuffer audioBuffer( new AudioBufferPrivate( audioDevice_, alBuffer ) );

	{
		QWriteLocker audioBuffersLocker( &audioBuffersMutex_ );
		audioBuffers_ << audioBuffer;
	}

	return audioBuffer;
}


void AudioDevicePrivate::removeBuffer( const AudioBuffer & audioBuffer )
{
	QWriteLocker audioBuffersLocker( &audioBuffersMutex_ );
	audioBuffers_.removeOne( audioBuffer );
}


void AudioDevicePrivate::loadBuffer( AudioBuffer audioBuffer, AudioBufferRequest * request, bool isPrioritized )
{
	Q_ASSERT( request->alBuffer.isNull() );
	Q_ASSERT( request->requestId == 0 );
	Q_ASSERT( !request->isActive );

	request->isActive = true;

	request->requestId = audioBuffer.d_->isStreaming() ?
		request->file ?
			bufferLoader_->addRequest( request->file,
				request->sampleOffset, request->sampleCount, false, isPrioritized ) :
			bufferLoader_->addRequest( audioBuffer.d_->fileName_, audioBuffer.d_->format_,
				request->sampleOffset, request->sampleCount, false, isPrioritized ) :
		bufferLoader_->addRequest( audioBuffer.d_->fileName_, audioBuffer.d_->format_,
			request->sampleOffset, request->sampleCount, true, isPrioritized );

	QWriteLocker locker( &audioBufferRequestsMutex_ );
	requestItemForId_[ request->requestId ] = RequestItem( audioBuffer, request );
}


void AudioDevicePrivate::increaseLoadPriority( int requestId )
{
	bufferLoader_->increasePriority( requestId );
}


void AudioDevicePrivate::cancelLoadRequest( int requestId )
{
	QWriteLocker audioBufferRequestsLocker( &audioBufferRequestsMutex_ );
	bufferLoader_->cancelRequest( requestId );
	requestItemForId_.remove( requestId );
}


void AudioDevicePrivate::_bufferFinishedForSource( AudioSource * audioSource )
{
	audioSource->d_->audioContext_->d_->addSourceForFinishedBuffer( audioSource );
}


void AudioDevicePrivate::_requestFinished( int requestId, void * resultp )
{
	AudioBufferLoader::Result * result = static_cast<AudioBufferLoader::Result*>( resultp );

	AudioContextLocker contextLocker = lock();

	QWriteLocker audioBufferRequestsLocker( &audioBufferRequestsMutex_ );
	Q_ASSERT( requestItemForId_.contains( requestId ) );

	RequestItem requestItem = requestItemForId_.take( requestId );

	QWriteLocker audioBufferLocker( &requestItem.audioBuffer.d_->mutex_ );

	// single audio source for streaming audio buffer
	AudioSource * singleAudioSource = 0;
	if ( requestItem.audioBuffer.d_->isStreaming() )
		singleAudioSource = requestItem.audioBuffer.d_->sourceForRequestId( requestId );

	requestItem.request->requestId = 0;
	requestItem.request->hasError = result->hasError;
	requestItem.request->content = result->content;
	requestItem.request->file = result->file;
	requestItem.request->isProcessed = false;

	if ( requestItem.audioBuffer.d_->isStreaming() )
	{
		_bufferFinishedForSource( singleAudioSource );
	}
	else
	{
		for ( QListIterator<AudioSource*> it( requestItem.audioBuffer.d_->toStatic()->audioSources ); it.hasNext(); )
			_bufferFinishedForSource( it.next() );
	}
}




/**
 * \class AudioDevice
 *
 * \ingroup audio_module
 *
 * \brief The AudioDevice class represents single entity for audio output.
 *
 * Depending on implementation device can be a physical hardware device, software daemon,
 * external server on network, etc.
 *
 * Constructing device instance means creating connection to particular service that maintains
 * audio output.
 *
 * All devices distinct by it's name and available device name for current host can be obtained with
 * AudioManager::availableDeviceNames().
 *
 * Use createContext() for constructing real instance of audio environment.
 *
 * \sa AudioManager::createDevice()
 */


/** \internal
 */

AudioDevice::AudioDevice( AudioDevicePrivate * d ) :
	d_( d )
{
	d_->audioDevice_ = this;
}


/**
 * Destroys instance of this audio device, breaking connection to audio output service.
 *
 * All constructed AudioContext instances must be destroyed explicitly before destroying device.
 */

AudioDevice::~AudioDevice()
{
	delete d_;
}


/**
 * Returns name of this device, that was previously passed to AudioManager::createDevice().
 */

QByteArray AudioDevice::name() const
{
	return d_->name_;
}


/**
 * Constructs audio context for this device with the given parameters.
 * Normally you don't need to change default parameters.
 * Leave -1 for any combination of parameters to tell device to choose default one.
 * Specifying invalid values for this device will cause fail of context creation.
 * In this case 0 will be returned.
 *
 * \a frequency is the value in Hz, that specifies audio output buffer frequency.
 *
 * \a refreshInterval is the value in Hz, that specifies refresh interval of context internal state.
 *
 * \a sync is a flag that indicates that this context should be synchronized.
 *
 * \a monoSources and \a stereoSources specifies respectively number of mono and stereo sources
 * that can be played at once.
 */

AudioContext * AudioDevice::createContext( int frequency, int refreshInterval, int sync, int monoSources, int stereoSources )
{
	return d_->createContext( frequency, refreshInterval, sync, monoSources, stereoSources );
}




} // namespace Grim
