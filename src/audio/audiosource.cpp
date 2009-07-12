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

#include "audiosource.h"

#include <QDebug>

#include <float.h>

#include "audio_p.h"
#include "audiocontext.h"
#include "audiodevice.h"
#include "audioformatplugin.h"




namespace Grim {




static const int QueueBufferSamples = 16384 * 4;




AudioSourcePrivate::AudioSourcePrivate( AudioContext * audioContext ) :
	audioContext_( audioContext ),
	areSignalsBlocked_( true ),
	inDestructor_( false ),
	isLooping_( false ),
	gain_( 1.0f ),
	minGain_( 0.0f ),
	maxGain_( 1.0f ),
	position_( 0.0f, 0.0f, 0.0f ),
	velocity_( 0.0f, 0.0f, 0.0f ),
	isRelativeToListener_( true ),
	pitch_( 1.0f ),
	direction_( 0.0f, 0.0f, 0.0f ),
	innerConeAngle_( 360.0f ),
	outerConeAngle_( 360.0f ),
	outerConeGain_( 1.0f ),
	referenceDistance_( 1.0f ),
	rolloffFactor_( 1.0f ),
	maxDistance_( FLT_MAX ),
	state_( AudioSource::State_Idle ),
	audioBuffer_( 0 )
{
	AudioContextLocker contextLocker = audioContext_->d_->lock();

	alSourceId_ = 0;
	alGenSources( 1, &alSourceId_ );
	Q_ASSERT_X( alSourceId_ != 0,
		"Grim::AudioSourcePrivate::AudioSourcePrivate()",
		"According to OpenAL 1.1 specifications this should not happen." );

	isActive_ = false;

	isInitialized_ = false;
	totalSamples_ = -1;
}


AudioSourcePrivate::~AudioSourcePrivate()
{
	AudioContextLocker contextLocker = audioContext_->d_->lock();

	inDestructor_ = true;

	// ensure our AudioBuffer instance will be destroyed after it's mutex will be unlocked
	AudioBuffer audioBufferCopy = audioBuffer_;

	if ( state_ != AudioSource::State_Idle )
	{
		Q_ASSERT( audioBuffer_.d_ );

		QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );

		stopSelf();
		deinitializeSelf();
	}

	// actual destruction point of our AudioBuffer
	audioBufferCopy = AudioBuffer();

	if ( audioSource_ != audioContext_->d_->currentDestructingAudioSource_ )
		audioContext_->d_->audioSources_.removeOne( audioSource_ );

	alDeleteSources( 1, &alSourceId_ );
	alSourceId_ = 0;
}


AudioBuffer AudioSourcePrivate::buffer() const
{
	return audioBuffer_;
}


void AudioSourcePrivate::setBuffer( const AudioBuffer & audioBuffer )
{
	AudioContextLocker contextLocker = audioContext_->d_->lock();

	// ensure our AudioBuffer instance will be destroyed after it's mutex will be unlocked
	AudioBuffer audioBufferCopy = audioBuffer_;

	if ( state_ != AudioSource::State_Idle )
	{
		Q_ASSERT( audioBuffer_.d_ );

		QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );

		stopSelf();

		if ( audioBuffer_ == audioBuffer )
		{
			// already playing exactly this buffer
			return;
		}

		deinitializeSelf();
	}

	// actual destruction point of our AudioBuffer
	audioBufferCopy = AudioBuffer();

	Q_ASSERT_X( !audioBuffer.d_ || audioBuffer.d_->audioDevice_ == audioContext_->d_->audioDevice_,
		"Grim::AudioSourcePrivate::setBuffer()",
		"Attempt to assign buffer from other AudioDevice." );

	if ( !audioBuffer.d_ )
		return;

	{
		QWriteLocker audioBufferLocker( &audioBuffer.d_->mutex_ );

		audioBuffer.d_->attachSource( audioSource_ );

		if ( audioBuffer_.d_->isStreaming() )
		{
			// two policies at this stage

#if 0
			// Policy 1: Do nothing. Relax and wait for play() call.
#endif

#if 1
			// Policy 2: Request zero-length buffer to gather data for _initializeQueue() call.
			AudioBufferRequest * request = audioBuffer_.d_->requestForSource( audioSource_ );
			_requestMore( request, true );
#endif
		}
		else
		{
			if ( !audioBuffer_.d_->toStatic()->request.alBuffer.isNull() )
				_initializeStatic();
		}
	}

	_setState( AudioSource::State_Stopped );
}


int AudioSourcePrivate::channelsCount() const
{
	if ( !isInitialized_ )
	{
		qWarning( "Grim::AudioSourcePrivate::channelsCount() : Source is not initialized yet." );
		return -1;
	}

	return channelsCount_;
}


int AudioSourcePrivate::bitsPerSample() const
{
	if ( !isInitialized_ )
	{
		qWarning( "Grim::AudioSourcePrivate::bitsPerSample() : Source is not initialized yet." );
		return -1;
	}

	return bitsPerSample_;
}


float AudioSourcePrivate::frequency() const
{
	if ( !isInitialized_ )
	{
		qWarning( "Grim::AudioSourcePrivate::frequency() : Source is not initialized yet." );
		return 0;
	}

	return frequency_;
}


qint64 AudioSourcePrivate::totalSamples() const
{
	if ( !isInitialized_ )
	{
		qWarning( "Grim::AudioSourcePrivate::totalSamples() : Source is not initialized yet." );
		return -1;
	}

	return totalSamples_;
}


qint64 AudioSourcePrivate::currentSampleOffset() const
{
	if ( !isInitialized_ )
	{
		qWarning( "Grim::AudioSourcePrivate::currentSampleOffset() : Source is not initialized yet." );
		return -1;
	}

	const qint64 offset = _calculateCurrentSampleOffset();
	return offset;
}


void AudioSourcePrivate::setCurrentSampleOffset( qint64 offset )
{
	if ( !isInitialized_ )
	{
		qWarning( "Grim::AudioSourcePrivate::setCurrentSampleOffset() : Source is not initialized yet." );
		return;
	}

	if ( isSequential_ )
	{
		qWarning( "Grim::AudioSourcePrivate::setCurrentSampleOffset() : Source is sequential and not seekable." );
		return;
	}

	if ( offset < 0 || offset > totalSamples_ )
	{
		qWarning( "Grim::AudioSourcePrivate::setCurrentSampleOffset() : Given sample offset is out of range." );
		return;
	}

	if ( offset == desiredSampleOffset_ )
		return;

	if ( offset == _calculateCurrentSampleOffset() )
		return;

	if ( offset == totalSamples_ )
	{
		if ( !isLooping_ )
		{
			QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );
			stopSelf();
			return;
		}

		offset = 0;
	}

	const qint64 previousCurrentSampleOffset = _calculateCurrentSampleOffset();

	desiredSampleOffset_ = offset;

	if ( desiredSampleOffset_ != -1 )
	{
		QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );
		if ( audioBuffer_.d_->isStreaming() )
			_seekQueue();
		else
			_seekStatic();
	}

	if ( !inDestructor_ && previousCurrentSampleOffset != _calculateCurrentSampleOffset() && !areSignalsBlocked_ )
		emit audioContext_->sourceCurrentOffsetChanged( audioSource_ );
}


inline void AudioSourcePrivate::_applyLooping()
{
	if ( !audioBuffer_.d_->isStreaming() )
		alSourcei( alSourceId_, AL_LOOPING, isLooping_ ? AL_TRUE : AL_FALSE );
}


inline void AudioSourcePrivate::_applyGain()
{
	alSourcef( alSourceId_, AL_GAIN, gain_ );
}


inline void AudioSourcePrivate::_applyMinGain()
{
	alSourcef( alSourceId_, AL_MIN_GAIN, minGain_ );
}


inline void AudioSourcePrivate::_applyMaxGain()
{
	alSourcef( alSourceId_, AL_MAX_GAIN, maxGain_ );
}


inline void AudioSourcePrivate::_applyPosition()
{
	AudioOpenALVector vector( position_ );
	alSourcefv( alSourceId_, AL_POSITION, vector.data );
}


inline void AudioSourcePrivate::_applyVelocity()
{
	AudioOpenALVector vector( velocity_ );
	alSourcefv( alSourceId_, AL_VELOCITY, vector.data );
}


inline void AudioSourcePrivate::_applyRelativeToListener()
{
	alSourcei( alSourceId_, AL_SOURCE_RELATIVE, isRelativeToListener_ ? AL_TRUE : AL_FALSE );
}


inline void AudioSourcePrivate::_applyPitch()
{
	alSourcef( alSourceId_, AL_PITCH, pitch_ );
}


inline void AudioSourcePrivate::_applyDirection()
{
	AudioOpenALVector vector( direction_ );
	alSourcefv( alSourceId_, AL_DIRECTION, vector.data );
}


inline void AudioSourcePrivate::_applyInnerConeAngle()
{
	alSourcef( alSourceId_, AL_CONE_INNER_ANGLE, innerConeAngle_ );
}


inline void AudioSourcePrivate::_applyOuterConeAngle()
{
	alSourcef( alSourceId_, AL_CONE_OUTER_ANGLE, outerConeAngle_ );
}


inline void AudioSourcePrivate::_applyOuterConeGain()
{
	alSourcef( alSourceId_, AL_CONE_OUTER_GAIN, outerConeGain_ );
}


inline void AudioSourcePrivate::_applyReferenceDistance()
{
	alSourcef( alSourceId_, AL_REFERENCE_DISTANCE, referenceDistance_ );
}


inline void AudioSourcePrivate::_applyRolloffFactor()
{
	alSourcef( alSourceId_, AL_ROLLOFF_FACTOR, rolloffFactor_ );
}


inline void AudioSourcePrivate::_applyMaxDistance()
{
	alSourcef( alSourceId_, AL_MAX_DISTANCE, maxDistance_ );
}


inline void AudioSourcePrivate::_applyAll()
{
	_applyLooping();
	_applyGain();
	_applyMinGain();
	_applyMaxGain();
	_applyPosition();
	_applyVelocity();
	_applyRelativeToListener();
	_applyPitch();
	_applyDirection();
	_applyInnerConeAngle();
	_applyOuterConeAngle();
	_applyOuterConeGain();
	_applyReferenceDistance();
	_applyRolloffFactor();
	_applyMaxDistance();
}


void AudioSourcePrivate::setLooping( bool set )
{
	if ( isLooping_ == set )
		return;

	isLooping_ = set;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyLooping();
	}
}


void AudioSourcePrivate::setGain( float gain )
{
	if ( gain_ == gain )
		return;

	gain_ = gain;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyGain();
	}
}


void AudioSourcePrivate::setMinGain( float gain )
{
	if ( minGain_ == gain )
		return;

	minGain_ = gain;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyMinGain();
	}
}


void AudioSourcePrivate::setMaxGain( float gain )
{
	if ( maxGain_ == gain )
		return;

	maxGain_ = gain;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyMaxGain();
	}
}


void AudioSourcePrivate::setPosition( const AudioVector & position )
{
	if ( position_ == position )
		return;

	position_ = position;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyPosition();
	}
}


void AudioSourcePrivate::setVelocity( const AudioVector & velocity )
{
	if ( velocity_ == velocity )
		return;

	velocity_ = velocity;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyVelocity();
	}
}


void AudioSourcePrivate::setRelativeToListener( bool set )
{
	if ( isRelativeToListener_ == set )
		return;

	isRelativeToListener_ = set;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyRelativeToListener();
	}
}


void AudioSourcePrivate::setPitch( float pitch )
{
	if ( pitch_ == pitch )
		return;

	pitch_ = pitch;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyPitch();
	}
}


void AudioSourcePrivate::setDirection( const AudioVector & direction )
{
	if ( direction_ == direction )
		return;

	direction_ = direction;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyDirection();
	}
}


void AudioSourcePrivate::setInnerConeAngle( float angle )
{
	if ( innerConeAngle_ == angle )
		return;

	innerConeAngle_ = angle;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyInnerConeAngle();
	}
}


void AudioSourcePrivate::setOuterConeAngle( float angle )
{
	if ( outerConeAngle_ == angle )
		return;

	outerConeAngle_ = angle;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyOuterConeAngle();
	}
}


void AudioSourcePrivate::setOuterConeGain( float gain )
{
	if ( outerConeGain_ == gain )
		return;

	outerConeGain_ = gain;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyOuterConeGain();
	}
}


void AudioSourcePrivate::setReferenceDistance( float distance )
{
	if ( referenceDistance_ == distance )
		return;

	referenceDistance_ = distance;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyReferenceDistance();
	}
}


void AudioSourcePrivate::setRolloffFactor( float factor )
{
	if ( rolloffFactor_ == factor )
		return;

	rolloffFactor_ = factor;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyRolloffFactor();
	}
}


void AudioSourcePrivate::setMaxDistance( float distance )
{
	if ( maxDistance_ == distance )
		return;

	maxDistance_ = distance;

	if ( isActive_ )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyMaxDistance();
	}
}


void AudioSourcePrivate::play()
{
	AudioContextLocker contextLocker = audioContext_->d_->lock();

	if ( !audioBuffer_.d_ )
	{
		Q_ASSERT( state_ == AudioSource::State_Idle );
		qWarning( "Grim::AudioSourcePrivate::play() : No buffer attached to play." );
		return;
	}

	Q_ASSERT( state_ != AudioSource::State_Idle );

	if ( state_ == AudioSource::State_Playing )
	{
		// already playing
		return;
	}

	QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );
	AudioBufferRequest * request = audioBuffer_.d_->requestForSource( audioSource_ );

	if ( !isInitialized_ )
	{
		if ( audioBuffer_.d_->isStreaming() )
		{
			// never ready on very first play() call
			_requestMore( request );
		}
		else
		{
			if ( request->alBuffer.isNull() )
				_requestMore( request );
			else
				_initializeStatic();
		}
	}

	if ( isInitialized_ )
	{
		if ( _isEmpty() )
		{
			_setState( AudioSource::State_Stopped );
			return;
		}

		if ( desiredSampleOffset_ == -1 && alBuffers_.isEmpty() )
		{
			_requestMore( request );
		}
		else
		{
			if ( desiredSampleOffset_ == -1 || _isOffsetInBounds( desiredSampleOffset_, false ) )
			{
				_setActive( true );
				if ( audioBuffer_.d_->isStreaming() )
					_playQueue();
				else
					_playStatic();
			}
		}
	}

	// all ok, going to playing state regardless when actual play will start, now or later
	_setState( AudioSource::State_Playing );
}


void AudioSourcePrivate::pause()
{
	AudioContextLocker contextLocker = audioContext_->d_->lock();

	if ( !audioBuffer_.d_ )
	{
		Q_ASSERT( state_ == AudioSource::State_Idle );
		qWarning( "Grim::AudioSourcePrivate::pause() : No buffer attached to pause." );
		return;
	}

	if ( state_ == AudioSource::State_Stopped || state_ == AudioSource::State_Paused )
		return;

	if ( isInitialized_ )
	{
		_setActive( false );
		if ( audioBuffer_.d_->isStreaming() )
			_pauseQueue();
		else
			_pauseStatic();
	}

	_setState( AudioSource::State_Paused );
}


void AudioSourcePrivate::stop()
{
	AudioContextLocker contextLocker = audioContext_->d_->lock();

	if ( !audioBuffer_.d_ )
	{
		Q_ASSERT( state_ == AudioSource::State_Idle );
		qWarning( "Grim::AudioSourcePrivate::pause() : No buffer attached to stop." );
		return;
	}

	if ( state_ == AudioSource::State_Stopped )
		return;

	QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );
	stopSelf();
}


void AudioSourcePrivate::setStaticOpenALBuffer( AudioBufferRequest * request )
{
	Q_ASSERT( state_ != AudioSource::State_Idle );
	Q_ASSERT( audioBuffer_.d_ );
	Q_ASSERT( !audioBuffer_.d_->isStreaming() );

	request->isActive = false;

	if ( request->alBuffer.isNull() && request->sampleCount != 0 )
	{
		// failed to create OpenAL buffer
		stopSelf();
		return;
	}

	_initializeStatic();

	if ( _isEmpty() )
	{
		_setState( AudioSource::State_Stopped );
		return;
	}

	if ( state_ == AudioSource::State_Stopped )
		return;

	if ( state_ == AudioSource::State_Paused )
		return;

	_setActive( true );
	_playStatic();
}


void AudioSourcePrivate::setQueueOpenALBuffer( AudioBufferRequest * request )
{
	Q_ASSERT( state_ != AudioSource::State_Idle );
	Q_ASSERT( audioBuffer_.d_ );
	Q_ASSERT( audioBuffer_.d_->isStreaming() );

	if ( request->alBuffer.isNull() && request->sampleCount != 0 )
	{
		// failed to create OpenAL buffer
		stopSelf();
		return;
	}

	if ( !isInitialized_ )
	{
		// initialize and enqueue this buffer as first
		_initializeQueue( request );
	}
	else
	{
		// already initialized, enqueue this buffer
		alBuffers_ << request->alBuffer;
		alSourceQueueBuffers( alSourceId_, 1, &request->alBuffer.id );

#ifdef GRIM_AUDIO_DEBUG
		ALint queuedBuffersCount;
		alGetSourcei( alSourceId_, AL_BUFFERS_QUEUED, &queuedBuffersCount );
		Q_ASSERT( queuedBuffersCount <= 2 ); // 2 == current buffer + next buffer
		Q_ASSERT( queuedBuffersCount == alBuffers_.count() );
#endif

		firstSampleOffset_ = alBuffers_.first().samplesOffset;
		lastSampleOffset_ = request->alBuffer.samplesOffset + request->alBuffer.samples;

		// no need more than 2 buffers in a queue
		Q_ASSERT( alBuffers_.count() <= 2 );
	}

	request->alBuffer = AudioOpenALBuffer();
	request->isActive = false;

	if ( isSequential_ )
	{
		if ( _isAtEnd( request ) )
			totalSamples_ = lastSampleOffset_;
	}

	const qint64 savedDesiredSampleOffset = desiredSampleOffset_;
	desiredSampleOffset_ = -1;

	if ( _isEmpty() )
	{
		_setState( AudioSource::State_Stopped );
		return;
	}

	if ( state_ == AudioSource::State_Stopped && request->alBuffer.isNull() )
	{
		// this request was just for gather data for _initializeQueue() call
		// do not request more
		return;
	}

	const bool isAtEnd = _isAtEnd( request );
	if ( isAtEnd )
	{
		Q_ASSERT( request->file );

		// we are at end of file stream
		// depending on params we should loop or stop source
		if ( isLooping_ )
		{
			// rewind
			request->file->device()->reset();
		}
		else
		{
			// should we reset or close?
			// reset will rewind audio file, but remain it open
			// close will require extra open call again in AudioBufferLoader
			request->file->device()->close();
		}
	}

	desiredSampleOffset_ = savedDesiredSampleOffset;

	if ( state_ == AudioSource::State_Stopped )
	{
		Q_ASSERT( firstSampleOffset_ != 0 );
	}
	else if ( state_ == AudioSource::State_Paused )
	{
		_seekToDesiredSampleOffset();
	}
	else if ( state_ == AudioSource::State_Playing )
	{
		if ( !isActive_ )
		{
			_setActive( true );
			_playQueue();
		}
		else
		{
			_seekToDesiredSampleOffset();
		}
	}

	desiredSampleOffset_ = -1;

	_requestMore( request );
}


void AudioSourcePrivate::stopSelf()
{
	if ( audioBuffer_.d_->isStreaming() )
		audioBuffer_.d_->clearQueueItemForSource( audioSource_, true );

	if ( isInitialized_ )
	{
		_setActive( false );

		const qint64 previousCurrentSampleOffset = _calculateCurrentSampleOffset();

		if ( audioBuffer_.d_->isStreaming() )
			_stopQueue();
		else
			_stopStatic();

		if ( !inDestructor_ )
		{
			const qint64 nowCurrentSampleOffset = _calculateCurrentSampleOffset();
			if ( previousCurrentSampleOffset != nowCurrentSampleOffset && !areSignalsBlocked_ )
				emit audioContext_->sourceCurrentOffsetChanged( audioSource_ );
		}
	}

	_setState( AudioSource::State_Stopped );
}


void AudioSourcePrivate::deinitializeSelf()
{
	if ( isInitialized_ )
	{
		if ( audioBuffer_.d_->isStreaming() )
			_deinitializeQueue();
		else
			_deinitializeStatic();
	}

	audioBuffer_.d_->detachSource( audioSource_ );

	_setState( AudioSource::State_Idle );
}


void AudioSourcePrivate::processSelf()
{
	Q_ASSERT( audioBuffer_.d_ );
	Q_ASSERT( state_ != AudioSource::State_Stopped );

	const qint64 previousCurrentSampleOffset = _calculateCurrentSampleOffset();

	if ( audioBuffer_.d_->isStreaming() )
	{
		ALint processedBuffersCount;
		ALint queuedBuffersCount;
		alGetSourcei( alSourceId_, AL_BUFFERS_PROCESSED, &processedBuffersCount );
		alGetSourcei( alSourceId_, AL_BUFFERS_QUEUED, &queuedBuffersCount );
		Q_ASSERT( processedBuffersCount <= alBuffers_.count() );

		if ( processedBuffersCount > 0 )
		{
			int i;

			AudioOpenALBuffer lastAlBuffer = alBuffers_.at( processedBuffersCount - 1 );
			const int lastSample = lastAlBuffer.samplesOffset + lastAlBuffer.samples;

			// unqueue processed buffers
			Q_ASSERT( processedBuffersCount <= 2 ); // 2 == current buffer + next buffer

			ALuint processedBuffersIds[ 2 ];
			alSourceUnqueueBuffers( alSourceId_, processedBuffersCount, processedBuffersIds );

			// destroy processed buffers
			AudioManagerPrivate * m = AudioManagerPrivate::sharedManagerPrivate();
			for ( i = 0; i < processedBuffersCount; ++i )
			{
				AudioOpenALBuffer alBuffer = alBuffers_.takeFirst();
				Q_ASSERT( processedBuffersIds[ i ] == alBuffer.id );
				m->destroyOpenALBuffer( alBuffer );
			}

			firstSampleOffset_ = lastSample;

			// check is source was looped
			if ( firstSampleOffset_ == totalSamples_ && !alBuffers_.isEmpty() )
				firstSampleOffset_ = 0;
		}

		// request more buffers
		{
			QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );
			_requestMore( audioBuffer_.d_->requestForSource( audioSource_ ) );
		}
	}

	if ( !alBuffers_.isEmpty() )
	{
		ALint sampleOffset;
		alGetSourceiv( alSourceId_, AL_SAMPLE_OFFSET, &sampleOffset );
		currentSampleOffset_ = sampleOffset;
	}
	else
	{
		currentSampleOffset_ = 0;
	}

	const qint64 nowCurrentSampleOffset = _calculateCurrentSampleOffset();
	if ( previousCurrentSampleOffset != nowCurrentSampleOffset && !areSignalsBlocked_ )
		emit audioContext_->sourceCurrentOffsetChanged( audioSource_ );

	ALint sourceState;
	alGetSourcei( alSourceId_, AL_SOURCE_STATE, &sourceState );

	if ( audioBuffer_.d_->isStreaming() )
	{
		if ( state_ == AudioSource::State_Playing && sourceState != AL_PLAYING && !alBuffers_.isEmpty() )
		{
			// restart playing if source was stopped automatically due to lack of buffers
			alSourcePlay( alSourceId_ );
			sourceState = AL_PLAYING;
		}

		if ( isLooping_ )
		{
			if ( sourceState == AL_STOPPED )
			{
				// stopped due to lack of buffers, will wait for more
			}
		}
		else
		{
			QWriteLocker audioBufferLocker( &audioBuffer_.d_->mutex_ );
			const bool isAtEnd = _isAtEnd( audioBuffer_.d_->requestForSource( audioSource_ ) );
			if ( alBuffers_.isEmpty() && isAtEnd && sourceState == AL_STOPPED )
			{
				// stopped normally, so change our state to Stopped also
				_setActive( false );
				_stopQueue();
				_setState( AudioSource::State_Stopped );
			}
		}
	}
	else
	{
		if ( isLooping_ )
		{
			Q_ASSERT( sourceState != AL_STOPPED );
		}
		else
		{
			if ( sourceState == AL_STOPPED )
			{
				_setActive( false );
				_setState( AudioSource::State_Stopped );
			}
		}
	}
}


void AudioSourcePrivate::_setInitialized( bool set )
{
	Q_ASSERT( isInitialized_ != set );

	isInitialized_ = set;

	if ( !inDestructor_ && !areSignalsBlocked_ )
		emit audioContext_->sourceInitializationChanged( audioSource_ );
}


void AudioSourcePrivate::_setState( AudioSource::State state )
{
	if ( state_ == state )
		return;

	state_ = state;

	if ( !inDestructor_ && !areSignalsBlocked_ )
		emit audioContext_->sourceStateChanged( audioSource_ );
}


void AudioSourcePrivate::_setActive( bool set )
{
	if ( isActive_ == set )
		return;

	isActive_ = set;

	if ( isActive_ )
	{
		_applyAll();
		audioContext_->d_->addActiveSource( audioSource_ );
	}
	else
	{
		audioContext_->d_->removeActiveSource( audioSource_ );
	}
}


bool AudioSourcePrivate::_isOffsetInBounds( qint64 offset, bool includingCurrentRequest ) const
{
	Q_ASSERT( !isSequential_ );

	AudioBufferRequest * request = audioBuffer_.d_->requestForSource( audioSource_ );
	const bool isAtEnd = _isAtEnd( request );

	const qint64 first = firstSampleOffset_;
	qint64 last = isAtEnd ? 0 : lastSampleOffset_;

	if ( includingCurrentRequest )
	{
		if ( request->isActive && request->sampleCount != -1 )
		{
			Q_ASSERT( request->sampleOffset == -1 || request->sampleOffset == lastSampleOffset_ );

			last = qMin( last + request->sampleCount, totalSamples_ );
		}
	}

	if ( last >= first )
		return offset >= first && offset < last;
	else
		return offset >= first || offset < last;
}


qint64 AudioSourcePrivate::_calculateCurrentSampleOffset() const
{
	Q_ASSERT( !_isEmpty() );

	if ( desiredSampleOffset_ != -1 )
		return desiredSampleOffset_;

	const qint64 absoluteSampleOffset = firstSampleOffset_ + currentSampleOffset_;
	return totalSamples_ == -1 ? absoluteSampleOffset : absoluteSampleOffset % totalSamples_;
}


bool AudioSourcePrivate::_isAtEnd( AudioBufferRequest * request ) const
{
	Q_ASSERT( isInitialized_ );

	if ( totalSamples_ != -1 )
		return lastSampleOffset_ == totalSamples_;

	if ( !request->file )
		return true;

	if ( !request->file->device()->isOpen() )
		return true;

	return request->file->device()->atEnd();
}


void AudioSourcePrivate::_requestMore( AudioBufferRequest * request, bool requestZeroSamples )
{
	// check is previous request is in progress
	if ( request->isActive )
		return;

	if ( audioBuffer_.d_->isStreaming() )
	{
		// no need for more than 2 buffers
		if ( alBuffers_.count() == 2 )
			return;

		if ( !isInitialized_ )
		{
			// request first buffer from very beginning of the stream
			request->sampleOffset = -1;
		}
		else
		{
			// check if we have desired position to play
			if ( desiredSampleOffset_ != -1 )
			{
				Q_ASSERT( alBuffers_.isEmpty() );
				Q_ASSERT( firstSampleOffset_ == lastSampleOffset_ );

				// mark first/last range as desired
				firstSampleOffset_ = desiredSampleOffset_;
				lastSampleOffset_ = desiredSampleOffset_;

				request->sampleOffset = desiredSampleOffset_;
			}
			else
			{
				const bool isAtEnd = _isAtEnd( request );

				// already loaded all buffers from stream
				if ( !isLooping_ && isAtEnd )
					return;

				// we are looped
				// if last loaded sample equals total samples - start from beginning
				request->sampleOffset = isAtEnd ? 0 : lastSampleOffset_;
			}
		}

		request->sampleCount = requestZeroSamples ? 0 : QueueBufferSamples;
	}
	else
	{
		request->sampleCount = -1;
	}

	audioBuffer_.d_->loadSelf( request, true );
}


void AudioSourcePrivate::_seekToDesiredSampleOffset()
{
	if ( desiredSampleOffset_ == -1 )
		return;

	Q_ASSERT( _isOffsetInBounds( desiredSampleOffset_, false ) );

	currentSampleOffset_ = desiredSampleOffset_ - firstSampleOffset_;
	desiredSampleOffset_ = -1;

	alSourcei( alSourceId_, AL_SAMPLE_OFFSET, currentSampleOffset_ );
}


void AudioSourcePrivate::_initializeStatic()
{
	Q_ASSERT( !isInitialized_ );
	Q_ASSERT( audioBuffer_.d_ );
	Q_ASSERT( !audioBuffer_.d_->isStreaming() );

	AudioOpenALBuffer alBuffer = audioBuffer_.d_->toStatic()->request.alBuffer;

	isSequential_ = false;
	channelsCount_ = alBuffer.channels;
	bitsPerSample_ = alBuffer.bitsPerSample;
	frequency_ = (float)alBuffer.frequency;
	totalSamples_ = alBuffer.samples;
	firstSampleOffset_ = 0;
	lastSampleOffset_ = alBuffer.samples;
	currentSampleOffset_ = 0;

	desiredSampleOffset_ = -1;

	alBuffers_ << alBuffer;

	if ( !alBuffer.isNull() )
		alSourcei( alSourceId_, AL_BUFFER, alBuffer.id );

	_setInitialized( true );
}


void AudioSourcePrivate::_deinitializeStatic()
{
	Q_ASSERT( isInitialized_ );

	alSourcei( alSourceId_, AL_BUFFER, 0 );

	totalSamples_ = -1;

	alBuffers_.clear();

	_setInitialized( false );
}


void AudioSourcePrivate::_playStatic()
{
	Q_ASSERT( alBuffers_.count() == 1 );
	Q_ASSERT( !alBuffers_.first().isNull() );

	alSourcePlay( alSourceId_ );

	_seekToDesiredSampleOffset();
}


void AudioSourcePrivate::_pauseStatic()
{
	Q_ASSERT( alBuffers_.count() == 1 );

	if ( !alBuffers_.first().isNull() )
		alSourcePause( alSourceId_ );
}


void AudioSourcePrivate::_stopStatic()
{
	alSourceStop( alSourceId_ );

	firstSampleOffset_ = 0;
	currentSampleOffset_ = 0;
	desiredSampleOffset_ = -1;
}


void AudioSourcePrivate::_seekStatic()
{
	currentSampleOffset_ = desiredSampleOffset_;
	desiredSampleOffset_ = -1;

	alSourcei( alSourceId_, AL_SAMPLE_OFFSET, currentSampleOffset_ );

	// can't be in stopped mode together with non-null currentSampleOffset_ value
	if ( state_ == AudioSource::State_Stopped )
		_setState( AudioSource::State_Paused );
}


void AudioSourcePrivate::_initializeQueue( AudioBufferRequest * request )
{
	Q_ASSERT( !isInitialized_ );
	Q_ASSERT( audioBuffer_.d_ );
	Q_ASSERT( audioBuffer_.d_->isStreaming() );

	isSequential_ = request->alBuffer.isSequential;
	channelsCount_ = request->alBuffer.channels;
	bitsPerSample_ = request->alBuffer.bitsPerSample;
	frequency_ = (float)request->alBuffer.frequency;
	totalSamples_ = request->alBuffer.totalSamples;
	firstSampleOffset_ = request->alBuffer.samplesOffset;
	lastSampleOffset_ = request->alBuffer.samplesOffset + request->alBuffer.samples;
	currentSampleOffset_ = 0;

	desiredSampleOffset_ = -1;

	if ( !_isEmpty() )
	{
		if ( request->alBuffer.isNull() )
		{
			// this was very first request just to initialize
		}
		else
		{
			alBuffers_ << request->alBuffer;
			alSourceQueueBuffers( alSourceId_, 1, &request->alBuffer.id );
		}
	}

	_setInitialized( true );
}


void AudioSourcePrivate::_deinitializeQueue()
{
	Q_ASSERT( isInitialized_ );

	alSourcei( alSourceId_, AL_BUFFER, 0 );

	totalSamples_ = -1;

	AudioManagerPrivate * m = AudioManagerPrivate::sharedManagerPrivate();
	for ( QListIterator<AudioOpenALBuffer> it( alBuffers_ ); it.hasNext(); )
	{
		const AudioOpenALBuffer & alBuffer = it.next();
		m->destroyOpenALBuffer( alBuffer );
	}
	alBuffers_.clear();

	_setInitialized( false );
}


void AudioSourcePrivate::_playQueue()
{
	alSourcePlay( alSourceId_ );

	_seekToDesiredSampleOffset();
}


void AudioSourcePrivate::_pauseQueue()
{
	alSourcePause( alSourceId_ );
}


void AudioSourcePrivate::_unqueueQueue()
{
	ALint processedBuffersCount;
	ALint queuedBuffersCount;

#ifdef GRIM_AUDIO_DEBUG
	alGetSourcei( alSourceId_, AL_BUFFERS_PROCESSED, &processedBuffersCount );
	alGetSourcei( alSourceId_, AL_BUFFERS_QUEUED, &queuedBuffersCount );
	Q_ASSERT( queuedBuffersCount <= 2 && queuedBuffersCount >= processedBuffersCount );
	Q_ASSERT( queuedBuffersCount == alBuffers_.count() );
#endif

	// stop source and mark all buffers as processed
	alSourceStop( alSourceId_ );

	alGetSourcei( alSourceId_, AL_BUFFERS_PROCESSED, &processedBuffersCount );
	alGetSourcei( alSourceId_, AL_BUFFERS_QUEUED, &queuedBuffersCount );
	if ( queuedBuffersCount > 0 && queuedBuffersCount != processedBuffersCount )
	{
		// Maybe bug of OpenAL design, buffers not marks as processed
		// if source was initially in AL_INITIAL or AL_STOP.
		// Setting null buffer detaches them, but we have no methods to obtain
		// queued buffer ids. Hopefully we have stored those ids in alBuffers_.
		alSourcei( alSourceId_, AL_BUFFER, 0 );
	}
	else
	{
#ifdef GRIM_AUDIO_NULL_BUFFER_WORKAROUND
		// Bug in openal-soft implementation.
		// Queued count equals to processed count at this stage.
		// After calling alSourcei( source_id, AL_BUFFER, 0 )
		// queued count becomes 0, but processed count stays as was.

		// unqeueue processed buffers manually
		ALuint processedBuffersIds[ 2 ];
		alSourceUnqueueBuffers( alSourceId_, alBuffers_.count(), processedBuffersIds );
#else
		// Creative Labs implementation have no this bug.

		// unqueue buffers automatically by single call
		alSourcei( alSourceId_, AL_BUFFER, 0 );
#endif
	}

#ifdef GRIM_AUDIO_DEBUG
	alGetSourcei( alSourceId_, AL_BUFFERS_PROCESSED, &processedBuffersCount );
	alGetSourcei( alSourceId_, AL_BUFFERS_QUEUED, &queuedBuffersCount );
	Q_ASSERT( queuedBuffersCount == 0 && processedBuffersCount == 0 );
#endif
}


void AudioSourcePrivate::_stopQueue()
{
	if ( firstSampleOffset_ != 0 )
	{
		// stopping means rewind first sample to 0, so current queue unuseless
		_clearQueue();
	}
	else
	{
		_unqueueQueue();

		// current queue buffers are still useful, reenqueue them
		for ( QListIterator<AudioOpenALBuffer> it( alBuffers_ ); it.hasNext(); )
		{
			const AudioOpenALBuffer & alBuffer = it.next();
			alSourceQueueBuffers( alSourceId_, 1, &alBuffer.id );
		}
	}

	currentSampleOffset_ = 0;
	desiredSampleOffset_ = -1;
}


void AudioSourcePrivate::_clearQueue()
{
	_unqueueQueue();

	// destroy buffers
	AudioManagerPrivate * m = AudioManagerPrivate::sharedManagerPrivate();
	for ( QListIterator<AudioOpenALBuffer> it( alBuffers_ ); it.hasNext(); )
	{
		const AudioOpenALBuffer & alBuffer = it.next();
		m->destroyOpenALBuffer( alBuffer );
	}
	alBuffers_.clear();

	firstSampleOffset_ = 0;
	lastSampleOffset_ = 0;
	currentSampleOffset_ = 0;
}


void AudioSourcePrivate::_seekQueue()
{
	if ( _isOffsetInBounds( desiredSampleOffset_, false ) )
	{
		if ( state_ == AudioSource::State_Stopped || state_ == AudioSource::State_Paused )
		{
			_setState( AudioSource::State_Paused );
			return;
		}

		_setActive( true );
		_playQueue();
	}
	else
	{
		// desired offset somewhere outside of current buffers
		// but maybe ongoing buffer from current request contains desired offset
		if ( _isOffsetInBounds( desiredSampleOffset_, true ) )
		{
			// yes, it is there, will wait until it arrives
		}
		else
		{
			// nope, so clear all buffers, current request and start from scratch

			audioBuffer_.d_->clearQueueItemForSource( audioSource_, false );

			_setActive( false );
			_clearQueue();

			_requestMore( audioBuffer_.d_->requestForSource( audioSource_ ) );
		}
	}
}




/**
 * \class AudioSource
 *
 * \ingroup audio_module
 *
 * \brief The AudioSource class represents single entity that plays sounds.
 *
 * \reentrant
 *
 * Each source belongs to concrete audio context, use AudioContext::createSource() to create
 * source instance.
 *
 * Sources are able to constantly move in 3D space, changes their's orientation and plays sounds.
 * Each source property can vary independently from others and will be applied when source became active.
 *
 * Source is not derived from QObject, but it can still emits signals via context, to whom it belongs.
 * By default all source's signals are blocked.
 * Use setSignalsBlocked() method to control whether context should emit signals for particular source.

 * To start playing audio buffer should be attached with setBuffer().
 * Then playing state is changed with three basic methods: play(), pause() and stop().
 * Additionally source emits AudioContext::sourceStateChanged() when playing state was changed spontaneously.
 * For example, when audio buffer was finished and source was not looped - it automatically switches state to State_Stopped.
 *
 * Source can stay in uninitialized state for some time while it collects data from audio buffer, if last
 * was not preloaded before.
 * Initialization flag can tested with isInitialized().
 * Apparently when source became initialized it emits AudioContext::sourceInitializationChanged() signal.
 * While source stays in uninitialized state several methods not allowed: isSequential(), channelsCount(),
 * bitsPerSample(), frequency(), currentSampleOffset(), setCurrentSampleOffset(), totalSamples(), firstSampleOffset,
 * lastSampleOffset().
 *
 * Regardless of initialization state if source has attached audio buffer - it is ready to issue playback calls:
 * play(), pause() and stop(). Real playback will start when source will accomplish initialization step.
 *
 * Sequential source means that user cannot scroll playing position.
 * This depends only on underlying storage type for audio data, for example if audio file stored in ZIP-archive.
 *
 * In all other cases source is able to scroll playing position either when it is paused or playing.
 *
 * All properties that controls source behavior in 3D environment relies to OpenAL 1.1 API.
 * Here is only brief explanation of each property.
 * Consult OpenAL specification for detailed description.
 *
 * \sa AudioContext::createSource()
 */


/**
 * \enum AudioSource::State
 *
 * This enum describes current state of audio source.
 */
/**\var AudioSource::State AudioSource::State_Idle
 * Source has no audio buffer attached. Implicitly this means that source is in uninitialized state.
 * All methods that controls state, play(), pause() and stop() are prohibited.
 */
/**\var AudioSource::State AudioSource::State_Paused
 * Source is somewhere in the middle of playing, but inactive.
 * Calling play() will resume playback from current position.
 * Calling stop() will rewind it to start.
 */
/**\var AudioSource::State AudioSource::State_Playing
 * Source is currently playing and active.
 * Calling pause() will preserve it current playback position and will make it inactive.
 * Calling stop() will stop it playback and rewind it to start.
 */
/**\var AudioSource::State AudioSource::State_Stopped
 * Source is at start of it audio buffer and inactive.
 * Calling play() will start playing from start.
 * Calling pause() does nothing.
 */


/** \internal
 */

AudioSource::AudioSource( AudioSourcePrivate * d ) :
	d_( d )
{
	d_->audioSource_ = this;
}


/**
 * Destroys source instance and detaches assigned audio buffer.
 * If no one else is using it's buffer and buffer has AudioBuffer::AutoUnload flag on -
 * it will be unloaded to release resources.
 */

AudioSource::~AudioSource()
{
	delete d_;
}


/**
 * Returns audio context instance to which this source belongs.
 *
 * \sa AudioContext::createSource()
 */

AudioContext * AudioSource::context() const
{
	return d_->audioContext_;
}


/**
 * Returns whether this audio source emits signals.
 * By default signals are blocked.
 *
 * \sa setSignalsBlocked()
 */

bool AudioSource::areSignalsBlocked() const
{
	return d_->areSignalsBlocked_;
}


/**
 * Changed whether this audio source should emit signals and returns previous blocking flag.
 *
 * \sa areSignalsBlocked()
 */

bool AudioSource::setSignalsBlocked( bool set )
{
	const bool savedAreSignalsBlocked = d_->areSignalsBlocked_;

	d_->areSignalsBlocked_ = set;

	return savedAreSignalsBlocked;
}


/**
 * Returns whether this audio source is initialized or not.
 * While source is uninitialized user cannot query several values:
 * isSequential(), channelsCount(), bitsPerSample(), frequency(), totalSamples(), firstSampleOffset,
 * lastSampleOffset(), currentSampleOffset() and setCurrentSampleOffset().
 *
 * \sa AudioContext::sourceInitializationChanged()
 */

bool AudioSource::isInitialized() const
{
	return d_->isInitialized_;
}


/**
 * Returns whether this source is sequential or not.
 * Sequential sources have no ability to change playback position with setCurrentSampleOffset().
 * Also it is not always possible to find out length of audio buffer, so totalSamples() can return -1.
 */

bool AudioSource::isSequential() const
{
	return d_->isSequential();
}


/**
 * Returns number of channels for audio buffer that this source is attached to.
 * Currently only mono and stereo audio buffers are possible.
 */

int AudioSource::channelsCount() const
{
	return d_->channelsCount();
}


/**
 * Returns number of bits per sample of audio buffer that this source is attached to.
 * Currently only 8 and 16 bits per sample are possible.
 */

int AudioSource::bitsPerSample() const
{
	return d_->bitsPerSample();
}


/**
 * Returns samples rate of audio buffer this source is attached to.
 * Returned value is in Hz.
 */

float AudioSource::frequency() const
{
	return d_->frequency();
}


/**
 * Returns total number of samples for audio buffer this source is attached to.
 * If total number of samples cannot be obtained this method returns -1.
 *
 * \sa currentSampleOffset()
 */

qint64 AudioSource::totalSamples() const
{
	return d_->totalSamples();
}


/**
 * Returns left bound of cached samples this source plays.
 * Static buffers are always cached fully, so for them this value is \c 0.
 * This is only usable for debugging purposes to see if source in near to overplay cached samples.
 *
 * \sa lastSampleOffset()
 */

qint64 AudioSource::firstSampleOffset() const
{
	return d_->firstSampleOffset_;
}


/**
 * Returns right bound of cached samples this source plays.
 * Static buffers are always cached fully, so for them this value equals to totalSamples().
 * This is only usable for debugging purposes to see if source in near to overplay cached samples.
 *
 * If lastSampleOffset() if lower than firstSampleOffset() - this means that source is looped and
 * part of cached samples are still at the end of stream, and part is at start.
 *
 * \sa firstSampleOffset()
 */

qint64 AudioSource::lastSampleOffset() const
{
	return d_->lastSampleOffset_;
}


/**
 * Return current playback position in samples.
 *
 * \sa setCurrentSampleOffset(), totalSamples()
 */

qint64 AudioSource::currentSampleOffset() const
{
	return d_->currentSampleOffset();
}


/**
 * Scroll playback position to \a offset.
 * This is not possible for sequential sources.
 *
 * \sa currentSampleOffset(), totalSamples()
 */

void AudioSource::setCurrentSampleOffset( qint64 offset )
{
	d_->setCurrentSampleOffset( offset );
}


/**
 * Returns audio buffer this source is attached to or null audio buffer if this source is in State_Idle.
 *
 * \sa setBuffer()
 */

AudioBuffer AudioSource::buffer() const
{
	return d_->buffer();
}


/**
 * Attached the given \a buffer to this source.
 * If the given audio buffer is null source goes into State_Idle.
 * Otherwise source instantly goes into State_Stop state and is ready to play.
 */

void AudioSource::setBuffer( const AudioBuffer & buffer )
{
	d_->setBuffer( buffer );
}


/**
 * Returns whether this source is looped.
 * By default sources are not looped.
 *
 * \sa setLooping()
 */

bool AudioSource::isLooping() const
{
	return d_->isLooping();
}


/**
 * Set that this source should start playback from start when audio buffer stream will be finished.
 *
 * \sa isLooping()
 */

void AudioSource::setLooping( bool set )
{
	d_->setLooping( set );
}


/**
 * Returns gain of this source in range \c [0..1].
 * If gain is \c 0 then source is completely muted.
 * By default gain is \c 1.
 *
 * \sa setGain()
 */

float AudioSource::gain() const
{
	return d_->gain();
}


/**
 * Set source gain to the given \a gain.
 *
 * \sa gain()
 */

void AudioSource::setGain( float gain )
{
	d_->setGain( gain );
}


/**
 * Returns minimum allowed gain for this source, which is always guaranteed
 * after effective gain value will be calculated.
 * By default minimal gain is \c 0.
 *
 * \openal
 *
 * \sa setMinGain(), maxGain()
 */

float AudioSource::minGain() const
{
	return d_->minGain();
}


/**
 * Set minimal allowed gain to the given \a gain.
 *
 * \openal
 *
 * \sa minGain(), setMaxGain()
 */

void AudioSource::setMinGain( float gain )
{
	d_->setMinGain( gain );
}


/**
 * Returns maxumin allowed gain for this source, which is always guaranteed
 * that it will not be exceed after effective gain value will be calculated.
 * By default maximum gain is \c 1.
 *
 * \openal
 *
 * \sa setMaxGain(), minGain()
 */

float AudioSource::maxGain() const
{
	return d_->maxGain();
}


/**
 * Set maximum allowed gain to the given \a gain.
 *
 * \openal
 *
 * \sa maxGain(), setMinGain()
 */

void AudioSource::setMaxGain( float gain )
{
	d_->setMaxGain( gain );
}


/**
 * Returns source position in 3D space.
 * By default all sources are in origin of 3D space (0,0,0).
 *
 * \sa setPosition(), AudioListener::position(), isRelativeToListener()
 */

AudioVector AudioSource::position() const
{
	return d_->position();
}


/**
 * Set source position in 3D space to be the given \a position.
 *
 * \sa position(), AudioListener::setPosition(), setRelativeToListener()
 */

void AudioSource::setPosition( const AudioVector & position )
{
	d_->setPosition( position );
}


/**
 * Returns velocity vector of this source.
 * Together with listener velocity and audio context properties it implies on Doppler effect.
 * By default source is not moving and returns zero vector.
 *
 * \openal
 *
 * \sa setVelocity(), AudioListener::velocity(), AudioContext::dopplerFactor(), AudioContext::speedOfSound()
 */

AudioVector AudioSource::velocity() const
{
	return d_->velocity();
}


/**
 * Set the velocity vector of this source to the given \a velocity.
 *
 * \openal
 *
 * \sa velocity(), AudioListener::setVelocity()
 */

void AudioSource::setVelocity( const AudioVector & velocity )
{
	d_->setVelocity( velocity );
}


/**
 * Returns whether source position is relative to the listener or absolute.
 * By default source coordinates are absolute and this method returns \c false.
 *
 * \sa setRelativeToListener(), position(), AudioListener::position()
 */

bool AudioSource::isRelativeToListener() const
{
	return d_->isRelativeToListener();
}


/**
 * Set whenther this position of this source should be treated as relative to the listener or
 * absolute.
 *
 * \sa isRelativeToListener(), position(), AudioListener::position()
 */

void AudioSource::setRelativeToListener( bool set )
{
	d_->setRelativeToListener( set );
}


/**
 * Returns pitch shift, or in other words - source playback speed.
 * Pitch value is in range \c (0..infinity).
 * Each reduction by half of this value equals pitch shift of -12 semotones (one octave reduction).
 * Each doubling of this value equals pitch shift +12 semitones (one octave increase).
 * By default pitch value is \c 1.
 *
 * \openal
 *
 * \sa setPitch()
 */

float AudioSource::pitch() const
{
	return d_->pitch();
}


/**
 * Set pitch value to the given \a pitch.
 *
 * \sa pitch()
 */

void AudioSource::setPitch( float pitch )
{
	d_->setPitch( pitch );
}


/**
 * Returns direction vector for this source.
 * Direction specifies vector of sound emission.
 * By default sources are not directional and zero vector returned.
 *
 * \openal
 *
 * \sa setDirection(), innerConeAngle(), outerConeAngle(), outerConeGain()
 */

AudioVector AudioSource::direction() const
{
	return d_->direction();
}


/**
 * Set direction vector of this source to the given \a direction.
 * If \a direction vector is zero vector, source became undirectional.
 *
 * \openal
 *
 * \sa direction(), setInnerConeAngle(), setOuterConeAngle(), setOuterConeGain()
 */

void AudioSource::setDirection( const AudioVector & direction )
{
	d_->setDirection( direction );
}


/**
 * Returns inner cone angle in degrees for directional sources.
 * By default inner cone angle is 360 degrees.
 *
 * \openal
 *
 * \sa setInnerConeAngle(), outerConeAngle(), outerConeGain()
 */

float AudioSource::innerConeAngle() const
{
	return d_->innerConeAngle();
}


/**
 * Set inner cone angle to be \a angle in degrees.
 *
 * \openal
 *
 * \sa innerConeAngle(), setOuterConeAngle(), setOuterConeGain()
 */

void AudioSource::setInnerCoreAngle( float angle )
{
	d_->setInnerConeAngle( angle );
}


/**
 * Returns outer cone angle in degrees for directional sources.
 * By default outer cone angle is 360 degrees.
 *
 * \openal
 *
 * \sa setOuterConeAngle(), innerConeAngle(), outerConeGain()
 */

float AudioSource::outerConeAngle() const
{
	return d_->outerConeAngle();
}


/**
 * Set outer cone angle to be \a angle in degrees.
 *
 * \openal
 *
 * \sa outerConeAngle(), setInnerConeAngle(), setOuterConeGain()
 */

void AudioSource::setOuterConeAngle( float angle )
{
	d_->setOuterConeAngle( angle );
}


/**
 * Returns gain value of outer cone.
 * Returned gain is in range \c [0..1] and valid only for directional sources.
 * By default outer gain is 1.
 *
 * \openal
 *
 * \sa setOuterConeGain(), innerConeAngle(), outerConeAngle()
 */

float AudioSource::outerConeGain() const
{
	return d_->outerConeGain();
}


/**
 * Set outer cone gain to be \a gain.
 *
 * \openal
 *
 * \sa outerConeGain(), setInnerConeAngle(), setOuterConeAngle()
 */

void AudioSource::setOuterConeGain( float gain )
{
	d_->setOuterConeGain( gain );
}


/**
 * Returns reference distance value for this source that will be used in distance attenuation calculations.
 *
 * \openal
 *
 * \sa rolloffFactor(), maxDistance(), AudioContext::distanceModel()
 */

float AudioSource::referenceDistance() const
{
	return d_->referenceDistance();
}


/**
 * Set reference diatnce value for this source to be \a distance.
 *
 * \openal
 *
 * \sa referenceDistance(), setRolloffFactor(), setMaxDistance(), AudioContext::setDistanceModel()
 */

void AudioSource::setReferenceDistance( float distance )
{
	d_->setReferenceDistance( distance );
}


/**
 * Returns rolloff factor for this source that will be used in distance attenuation calculations.
 *
 * \openal
 *
 * \sa setRolloffFactor(), referenceDistance(), maxDistance(), AudioContext::distanceModel()
 */

float AudioSource::rolloffFactor() const
{
	return d_->rolloffFactor();
}


/**
 * Set rolloff factor for this source to be \a factor.
 *
 * \openal
 *
 * \sa rolloffFactor(), setReferenceDistance(), setMaxDistance(), AudioContext::setDistanceModel()
 */

void AudioSource::setRolloffFactor( float factor )
{
	d_->setRolloffFactor( factor );
}


/**
 * Returns maximum distance value for this source that will be used in distance attenuation calculations.
 *
 * \openal
 *
 * \sa setMaxDistance(), referenceDistance(), rolloffFactor(), AudioContext::distanceModel()
 */

float AudioSource::maxDistance() const
{
	return d_->maxDistance();
}


/**
 * Set maximum distance value for this source to be \a distance.
 *
 * \openal
 *
 * \sa maxDistance(), setReferenceDistance(), setRolloffFactor(), AudioContext::setDistanceModel()
 */

void AudioSource::setMaxDistance( float distance )
{
	d_->setMaxDistance( distance );
}


/**
 * Returns current state.
 */

AudioSource::State AudioSource::state() const
{
	return d_->state();
}


/**
 * Starts playback and switches source state to State_PLaying.
 *
 * \sa pause(), stop()
 */

void AudioSource::play()
{
	d_->play();
}


/**
 * Pauses playback and switches source state to State_Paused.
 *
 * \sa play(), stop()
 */

void AudioSource::pause()
{
	d_->pause();
}


/**
 * Stops playback, rewinding to the start of the stream and switches source state to State_Stopped.
 *
 * \sa play(), pause()
 */

void AudioSource::stop()
{
	d_->stop();
}




} // namespace Grim
