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

#include "audiolistener.h"

#include "audio_p.h"
#include "audiocontext.h"




namespace Grim {




AudioListenerPrivate::AudioListenerPrivate( AudioContext * audioContext ) :
	audioContext_( audioContext ),
	gain_( 1.0f ),
	position_( 0.0f, 0.0f, 0.0f ),
	velocity_( 0.0f, 0.0f, 0.0f ),
	orientationAt_( 0.0f, 0.0f, -1.0f ),
	orientationUp_( 0.0f, 1.0f, 0.0f )
{
}


AudioListenerPrivate::~AudioListenerPrivate()
{
	Q_ASSERT_X( audioListener_ != audioContext_->d_->defaultAudioListener_,
		"Grim::AudioListenerPrivate::~AudioListenerPrivate()",
		"Destroying default listener is prohibited" );

	// if this listener was current then set default listener as current
	if ( audioListener_ == audioContext_->d_->currentAudioListener_ && audioContext_->d_->defaultAudioListener_ )
	{
		audioContext_->d_->currentAudioListener_ = audioContext_->d_->defaultAudioListener_;
		audioContext_->d_->currentAudioListener_->d_->_applyAll();
	}

	if ( audioListener_ != audioContext_->d_->currentDestructingAudioListener_ )
		audioContext_->d_->audioListeners_.removeOne( audioListener_ );
}


inline void AudioListenerPrivate::_applyGain()
{
	alListenerf( AL_GAIN, gain_ );
}


inline void AudioListenerPrivate::_applyPosition()
{
	AudioOpenALVector vector( position_ );
	alListenerfv( AL_POSITION, vector.data );
}


inline void AudioListenerPrivate::_applyVelocity()
{
	AudioOpenALVector vector( velocity_ );
	alListenerfv( AL_VELOCITY, vector.data );
}


inline void AudioListenerPrivate::_applyOrientation()
{
	AudioOpenALVector vector( orientationAt_, orientationUp_ );
	alListenerfv( AL_ORIENTATION, vector.data );
}


inline void AudioListenerPrivate::_applyAll()
{
	_applyGain();
	_applyPosition();
	_applyVelocity();
	_applyOrientation();
}


bool AudioListenerPrivate::isCurrent() const
{
	return audioContext_->d_->currentAudioListener_ == audioListener_;
}


void AudioListenerPrivate::setCurrent()
{
	if ( audioContext_->d_->currentAudioListener_ == audioListener_ )
		return;

	audioContext_->d_->currentAudioListener_ = audioListener_;
	audioContext_->d_->currentAudioListener_->d_->_applyAll();
}


void AudioListenerPrivate::setGain( float gain )
{
	if ( gain_ == gain )
		return;

	gain_ = gain;

	if ( isCurrent() )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyGain();
	}
}


void AudioListenerPrivate::setPosition( const AudioVector & position )
{
	if ( position_ == position )
		return;

	position_ = position;

	if ( isCurrent() )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyPosition();
	}
}


void AudioListenerPrivate::setVelocity( const AudioVector & velocity )
{
	if ( velocity_ == velocity )
		return;

	velocity_ = velocity;

	if ( isCurrent() )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyVelocity();
	}
}


void AudioListenerPrivate::setOrientation( const AudioVector & at, const AudioVector & up )
{
	if ( orientationAt_ == at && orientationUp_ == up )
		return;

	orientationAt_ = at;
	orientationUp_ = up;

	if ( isCurrent() )
	{
		AudioContextLocker contextLocker = audioContext_->d_->lock();
		_applyOrientation();
	}
}




/**
 * \class AudioListener
 *
 * \ingroup audio_module
 *
 * \brief The AudioListener class represents listener of audio sources.
 *
 * \reentrant
 *
 * Normally there is a single current audio listener per audio context.
 * By audio context construction default one will be created for you automatically.
 *
 * You can create as many additional custom listeners as you want with AudioContext::createListener() and
 * switch between them while context is running.
 *
 * \sa AudioContext::currentListener()
 */


/** \internal
 */

AudioListener::AudioListener( AudioListenerPrivate * d ) :
	d_( d )
{
	d_->audioListener_ = this;
}


/**
 * Destroys custom audio listener instance.
 * If this listener was current then a default one with be set as current.
 * Destroying default audio listener is prohibited.
 * If your own custom listener instances was not destroyed on audio context destruction -
 * it will destroy them automatically.
 */

AudioListener::~AudioListener()
{
	delete d_;
}


/**
 * Returns whether this listener is set as current for audio context.
 * All operations on non current listeners are valid and will be applied next time listener
 * will be set as current.
 *
 * \sa setCurrent(), AudioContext::currentListener()
 */

bool AudioListener::isCurrent() const
{
	return d_->isCurrent();
}


/**
 * Set this listener as current for audio context.
 *
 * \sa isCurrent()
 */

void AudioListener::setCurrent()
{
	d_->setCurrent();
}


/**
 * Returns audio context instance this listener belongs to.
 *
 * \sa AudioContext::createListener()
 */

AudioContext * AudioListener::context() const
{
	return d_->audioContext_;
}


/**
 * Returns gain of this listener in range \c [0..1].
 * You can think about gain as about master volume, where \c 0 is completely muted, while \c 1 is the maximum volume.
 * By default gain equals to \a 1.
 *
 * \sa setGain()
 */

float AudioListener::gain() const
{
	return d_->gain();
}


/**
 * Set the gain of this listener to be the given \a gain.
 *
 * \sa gain()
 */

void AudioListener::setGain( float gain )
{
	d_->setGain( gain );
}


/**
 * Returns position of this listener in 3D space.
 * This position will be calculated as relative to the audio sources.
 * By default listener is at origin of 3D space (0,0,0).
 *
 * \sa setPosition(), AudioSource::position(), AudioSource::isRelativeToListener()
 */

AudioVector AudioListener::position() const
{
	return d_->position();
}


/**
 * Set position in 3D space of this listener to be the given \a position.
 *
 * \sa position(), AudioSource::setPosition(), AudioSource::isRelativeToListener()
 */

void AudioListener::setPosition( const AudioVector & position )
{
	d_->setPosition( position );
}


/**
 * Returns velocity vector of this listener.
 * Together with audio sources velocities and audio context properties it implies on Doppler effect.
 * By default listener is not moving and returns zero vector.
 *
 * \openal
 *
 * \sa setVelocity(), AudioSource::velocity(), AudioContext::dopplerFactor(), AudioContext::speedOfSound()
 */

AudioVector AudioListener::velocity() const
{
	return d_->velocity();
}


/**
 * Set the velocity vector of this listener to the given \a velocity.
 *
 * \openal
 *
 * \sa velocity(), AudioSource::setVelocity()
 */

void AudioListener::setVelocity( const AudioVector & velocity )
{
	d_->setVelocity( velocity );
}


/**
 * Returns orientation vector this listener is looking at.
 * By default listener is looking forward to the -z coordinate (0,0,-1).
 *
 * \sa setOrientation(), orientationUp()
 */

AudioVector AudioListener::orientationAt() const
{
	return d_->orientationAt();
}


/**
 * Returns orientation vector this listener's "head" is looking up to.
 * By default listener's "head" is up to the sky +y coordinate (0,1,0).
 *
 * \sa setOrientation(), orientationAt()
 */

AudioVector AudioListener::orientationUp() const
{
	return d_->orientationUp();
}


/**
 * Set this listener orientation where it look \a at and how it's "head" is \a up to.
 *
 * \sa orientationAt(), orientationUp()
 */

void AudioListener::setOrientation( const AudioVector & at, const AudioVector & up )
{
	d_->setOrientation( at, up );
}




} // namespace Grim
