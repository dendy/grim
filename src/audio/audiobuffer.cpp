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

#include "audiobuffer.h"

#include <QDebug>

#include "audio_p.h"
#include "audiodevice.h"
#include "audiocontext.h"
#include "audioformatplugin.h"




namespace Grim {




/** \internal
 * \class AudioBufferPrivate
 */


AudioBufferPrivate::AudioBufferPrivate( AudioDevice * audioDevice,
	const QString & fileName, const QByteArray & format, AudioBuffer::Policy policy ) :
	audioDevice_( audioDevice ),
	fileName_( fileName ), format_( format ), policy_( policy ),
	removingFromAudioDeviceFlag_( false ),
	data_( 0 )
{
	_init();
}


AudioBufferPrivate::AudioBufferPrivate( AudioDevice * audioDevice, const AudioOpenALBuffer & alBuffer ) :
	audioDevice_( audioDevice ),
	policy_( AudioBuffer::NoPolicy ),
	removingFromAudioDeviceFlag_( false ),
	data_( 0 )
{
	_init();

	AudioBufferStatic * staticData = toStatic();
	staticData->request.alBuffer = alBuffer;
}


AudioBufferPrivate::~AudioBufferPrivate()
{
	// at this point we don't need to lock our mutex
	// because all references to this buffer are cleared

	if ( isStreaming() )
	{
		AudioBufferQueue * queueData = toQueue();

		Q_ASSERT( queueData->items.isEmpty() );

		delete queueData;
	}
	else
	{
		AudioBufferStatic * staticData = toStatic();
		Q_ASSERT( staticData->request.alBuffer.isNull() );
		Q_ASSERT( staticData->request.requestId == 0 );
		Q_ASSERT( staticData->audioSources.isEmpty() );

		_clearStaticData();

		delete staticData;
	}
}


/**
 * Constructs self and loads buffer instantly if necessary flags matches.
 */

void AudioBufferPrivate::_init()
{
	if ( isStreaming() )
	{
		AudioBufferQueue * queueData = new AudioBufferQueue;
		data_ = queueData;
	}
	else
	{
		AudioBufferStatic * staticData = new AudioBufferStatic;
		data_ = staticData;
	}

	QWriteLocker locker( &mutex_ );

	// check if buffer should actually loaded at runtime
	if ( policy_ & AudioBuffer::LoadOnDemand || policy_ & AudioBuffer::Streaming )
		return;

	// buffer should be loaded right now
	AudioBufferRequest * request = &toStatic()->request;
	loadSelf( request, false );
}


/**
 * Lookups for the queue item for the given \a audioSource and clears it.
 * The \a audioSource must be valid.
 */

void AudioBufferPrivate::clearQueueItemForSource( AudioSource * audioSource, bool deleteFile )
{
	Q_ASSERT( isStreaming() );

	for ( QMutableListIterator<AudioBufferQueueItem> it( toQueue()->items ); it.hasNext(); )
	{
		AudioBufferQueueItem & queueItem = it.next();
		if ( queueItem.audioSource == audioSource )
		{
			_clearQueueItem( &queueItem, deleteFile );
			return;
		}
	}

	Q_ASSERT( false );
}


/**
 * Returns request pointer for the given \a audioSource.
 * If this is a static buffer - return shared request.
 * If this is a streaming buffer - returns request from the queueItem for the given \a audioSource.
 * The \a audioSource must be valid.
 */

AudioBufferRequest * AudioBufferPrivate::requestForSource( AudioSource * audioSource )
{
	if ( !isStreaming() )
	{
		Q_ASSERT( toStatic()->audioSources.contains( audioSource ) );
		return &toStatic()->request;
	}

	for ( QMutableListIterator<AudioBufferQueueItem> it( toQueue()->items ); it.hasNext(); )
	{
		AudioBufferQueueItem & queueItem = it.next();
		if ( queueItem.audioSource == audioSource )
			return &queueItem.request;
	}

	Q_ASSERT( false );
	return 0;
}


/**
 * Returns audio source that caused request with the \a requestId.
 * This buffer must be streaming.
 */

AudioSource * AudioBufferPrivate::sourceForRequestId( int requestId )
{
	Q_ASSERT( isStreaming() );

	for ( QListIterator<AudioBufferQueueItem> it( toQueue()->items ); it.hasNext(); )
	{
		const AudioBufferQueueItem & queueItem = it.next();
		if ( queueItem.request.requestId == requestId )
			return queueItem.audioSource;
	}

	Q_ASSERT( false );
	return 0;
}


/**
 * Initiates load request for the given \a request.
 * If \a isPrioritized is set - request priority will be increased.
 * \a isPrioritized is normally set from the audio source, when it wants to play as soon as possible.
 */

void AudioBufferPrivate::loadSelf( AudioBufferRequest * request, bool isPrioritized )
{
	Q_ASSERT( !request->isActive );
	audioDevice_->d_->loadBuffer( AudioBuffer( this ), request, isPrioritized );
}


/**
 * For the static buffer this appends the given \a audioSource to the list of static data.
 * For the streaming buffer this constructs queueItem for the given \a audioSource.
 */

void AudioBufferPrivate::attachSource( AudioSource * audioSource )
{
	Q_ASSERT( audioSource->d_->audioBuffer_.d_ != this );

	if ( isStreaming() )
	{
		AudioBufferQueue * queueData = toQueue();

		AudioBufferQueueItem queueItem;
		queueItem.audioSource = audioSource;

		queueData->items << queueItem;
	}
	else
	{
		AudioBufferStatic * staticData = toStatic();

		Q_ASSERT( !staticData->audioSources.contains( audioSource ) );

		staticData->audioSources << audioSource;
	}

	audioSource->d_->audioBuffer_ = AudioBuffer( this );
}


/**
 * Buffer must be static.
 * Destroys OpenAL buffer if was one.
 */

void AudioBufferPrivate::_clearStaticData()
{
	AudioBufferStatic * staticData = toStatic();

	if ( !staticData->request.alBuffer.isNull() )
	{
		Q_ASSERT( !staticData->request.isActive );
		AudioContextLocker contextLocker = audioDevice_->d_->lock();
		AudioManagerPrivate::sharedManagerPrivate()->destroyOpenALBuffer( staticData->request.alBuffer );
		staticData->request.alBuffer = AudioOpenALBuffer();
		staticData->request.isProcessed = false;
	}
}


/**
 * Cancels request and deletes file instance for the \a queueItem.
 */

void AudioBufferPrivate::_clearQueueItem( AudioBufferQueueItem * queueItem, bool deleteFile )
{
	if ( queueItem->request.isActive )
	{
		if ( queueItem->request.requestId != 0 )
			audioDevice_->d_->cancelLoadRequest( queueItem->request.requestId );
		else
			queueItem->audioSource->d_->audioContext_->d_->removeSourceForFinishedBuffer( queueItem->audioSource );
		queueItem->request = AudioBufferRequest();
		queueItem->request.isActive = false;
	}
	else
	{
		if ( deleteFile && queueItem->request.file )
		{
			delete queueItem->request.file;
			queueItem->request.file = 0;
		}
	}
}


/**
 * For the static buffer this removes the given \a audioSource from the static data list.
 * For the streaming buffer this destroys queueItem for the given \a audioSource.
 * This also unloads OpenAL buffer is AutoUnload flag was set and no one references to this buffer.
 */

void AudioBufferPrivate::detachSource( AudioSource * audioSource )
{
	Q_ASSERT( audioSource->d_->audioBuffer_.d_ == this );

	if ( isStreaming() )
	{
		AudioBufferQueue * queueData = toQueue();

		bool isFound = false;
		for ( QMutableListIterator<AudioBufferQueueItem> it( queueData->items ); it.hasNext(); )
		{
			AudioBufferQueueItem & queueItem = it.next();
			if ( queueItem.audioSource == audioSource )
			{
				_clearQueueItem( &queueItem, true );
				it.remove();
				isFound = true;
				break;
			}
		}
		Q_ASSERT( isFound );
	}
	else
	{
		AudioBufferStatic * staticData = toStatic();

		Q_ASSERT( staticData->audioSources.contains( audioSource ) );

		staticData->audioSources.removeOne( audioSource );

		if ( staticData->request.isActive && staticData->request.requestId == 0 )
			audioSource->d_->audioContext_->d_->removeSourceForFinishedBuffer( audioSource );

		if ( staticData->audioSources.isEmpty() && (policy_ & AudioBuffer::AutoUnload) )
			_clearStaticData();
	}

	audioSource->d_->audioBuffer_ = AudioBuffer();
}




inline void AudioBufferPrivate::tryRemoveSelfFromDevice()
{
	// hack to automatically remove last instance from AudioDevice cache
	// 2 refs == this instance + instance in AudioDevice cache
	// this compare is actually thread-safe, because we operate on copy instance

	if ( !this )
		return;

	if ( ref != 2 || removingFromAudioDeviceFlag_ )
		return;

	removingFromAudioDeviceFlag_ = true;
	audioDevice_->d_->removeBuffer( AudioBuffer( this ) );
}




/**
 * \class AudioBuffer
 *
 * \ingroup audio_module
 *
 * \brief The AudioBuffer class references to sound data source.
 *
 * \threadsafe
 *
 * This explicitly shared class represents storage of sound data, that can be played with AudioSource.
 *
 * Buffer instances are constructed with AudioContext::createBuffer() and shared between all contexts
 * belongs to the parent audio device.
 *
 * On construction policy flags are provided, which describes how exactly this buffer should be used.
 *
 * All buffer instances must be destroyed explicitly before audio devices that maintains it will be destroyed.
 * Local buffer copy can be destroyed by assigning it to null buffer.
 *
 * <b>Static buffers -vs- Streaming buffers</b>
 *
 * Buffers can be in one of two modes: static ot streaming.
 *
 * Static buffers means that buffer will be completely loaded into memory, providing instant playback operations.
 * This eats memory to hold buffer data and CPU time for decoding at startup.
 *
 * Streaming buffers in opposite will be loaded in memory part by part while playback already runs.
 * This consumes dramatically less memory, but required constant CPU time for decoding parts of sound stream.
 *
 * Generally, static buffers should be used to play numerous small sounds, that repeats every time,
 * while streaming buffers are much suitable to play long sound tracks, that loops or repeats rarely.
 *
 * \sa AudioContext::createBuffer(), AudioSource::setBuffer()
 */


/**
 * \enum AudioBuffer::Policy
 *
 * This enum is a collection of flags which describes buffer behavior at run time.
 */
/**\var AudioBuffer::Policy AudioBuffer::NoPolicy
 * Buffer is static, it will be loaded instantly in memory and unloaded only on destruction.
 */
/**\var AudioBuffer::Policy AudioBuffer::LoadOnDemand
 * Buffer will not be loaded until first attempt from audio source to play it.
 */
/**\var AudioBuffer::Policy AudioBuffer::AutoUnload
 * Buffer will be unloaded from memory automatically when last audio source will stop playing it.
 */
/**\var AudioBuffer::Policy AudioBuffer::Streaming
 * Buffer is in streaming mode, is will be loaded into memory sequentially part by part while playing.
 */


/**
 * Constructs null buffer.
 * Use this to pass to AudioSource::setBuffer() to detach is from his buffer.
 */

AudioBuffer::AudioBuffer()
{
}


/** \internal
 */

AudioBuffer::AudioBuffer( AudioBufferPrivate * d ) :
	d_( d )
{
}


/**
 * Constructs reference to the given \a audioBuffer.
 */

AudioBuffer::AudioBuffer( const AudioBuffer & audioBuffer ) :
	d_( audioBuffer.d_ )
{
}


/**
 * Assigns self to the given \a audioBuffer.
 */

AudioBuffer & AudioBuffer::operator=( const AudioBuffer & audioBuffer )
{
	QExplicitlySharedDataPointer<AudioBufferPrivate> dCopy = d_;

	d_ = audioBuffer.d_;

	dCopy->tryRemoveSelfFromDevice();

	return *this;
}


/** \internal
 */

AudioBuffer & AudioBuffer::operator=( AudioBufferPrivate * d )
{
	QExplicitlySharedDataPointer<AudioBufferPrivate> dCopy = d_;

	d_ = d;

	dCopy->tryRemoveSelfFromDevice();

	return *this;
}


/**
 * Destroys reference to buffer.
 */

AudioBuffer::~AudioBuffer()
{
	d_->tryRemoveSelfFromDevice();
}


/**
 * Returns file name where this buffer will obtain data from.
 *
 * \sa format()
 */

QString AudioBuffer::fileName() const
{
	return d_->fileName_;
}


/**
 * Returns audio format name that for the file this buffer refers to
 * or null name if format should be guessed automatically.
 *
 * \sa fileName()
 */

QByteArray AudioBuffer::format() const
{
	return d_->format_;
}


/**
 * Returns set of flags, that describes how this buffer will be used.
 */

AudioBuffer::Policy AudioBuffer::policy() const
{
	return d_->policy_;
}




} // namespace Grim
