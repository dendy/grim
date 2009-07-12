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

#include "audiocontext.h"

#include <QCoreApplication>
#include <QDebug>

#include "audio_p.h"
#include "audiomanager.h"
#include "audiolistener.h"
#include "audiosource.h"
#include "audiodevice.h"




namespace Grim {




// timeout interval to update audio sources state from OpenAL state machine, in ms
static const int ActiveAudioSourcesTimeout = 100;




/** \internal
 * \class AudioContextPrivate
 */


AudioContextPrivate::AudioContextPrivate( AudioDevice * audioDevice, ALCcontext * alcContext ) :
	audioDevice_( audioDevice ), alcContext_( alcContext )
{
	isProcessing_ = true;

	// OpenAL default setting according to OpenAL 1.1 specification
	distanceModel_ = AudioContext::DistanceModel_InverseClamped;
	dopplerFactor_ = 1.0f;
	speedOfSound_ = 343.3f;

	currentAudioListener_ = 0;

	currentDestructingAudioSource_ = 0;
	currentDestructingAudioListener_ = 0;

	finishedAudioSourcesPosted_ = false;
}


AudioContextPrivate::~AudioContextPrivate()
{
	AudioContextLocker contextLocker = lock();

	// clear sources
	{
		for ( QListIterator<AudioSource*> it( audioSources_ ); it.hasNext(); )
		{
			currentDestructingAudioSource_ = it.next();
			delete currentDestructingAudioSource_;
		}
		audioSources_.clear();
		currentDestructingAudioSource_ = 0;
	}

	// clear listeners
	{
		// mark default listener as null to avoid assert in it's destructor
		defaultAudioListener_ = 0;
		for ( QListIterator<AudioListener*> it( audioListeners_ ); it.hasNext(); )
		{
			currentDestructingAudioListener_ = it.next();
			delete currentDestructingAudioListener_;
		}
		audioListeners_.clear();
		currentDestructingAudioListener_ = 0;
	}

	AudioManagerPrivate::sharedManagerPrivate()->unsetCurrentContextIfCurrent( audioContext_ );

	alcDestroyContext( alcContext_ );
	alcContext_ = 0;

	audioDevice_->d_->removeContext( audioContext_ );
}


/**
 * Set this context current and returns entity that prevents OpenAL context
 * switching from other threads until will not be destructed.
 */

AudioContextLocker AudioContextPrivate::lock()
{
	return AudioManagerPrivate::sharedManagerPrivate()->lockAudioContext( audioContext_ );
}


void AudioContextPrivate::setProcessing( bool set )
{
	if ( isProcessing_ == set )
		return;

	isProcessing_ = set;

	if ( isProcessing_ )
		alcProcessContext( alcContext_ );
	else
		alcSuspendContext( alcContext_ );

	_updateActiveAudioSourcesTimer();
}


void AudioContextPrivate::setDistanceModel( AudioContext::DistanceModel distanceModel )
{
	if ( distanceModel_ == distanceModel )
		return;

	distanceModel_ = distanceModel;

	ALenum alModel = 0;
	switch ( distanceModel_ )
	{
	case AudioContext::DistanceModel_None:
		alModel = AL_NONE;
		break;
	case AudioContext::DistanceModel_Inverse:
		alModel = AL_INVERSE_DISTANCE;
		break;
	case AudioContext::DistanceModel_InverseClamped:
		alModel = AL_INVERSE_DISTANCE_CLAMPED;
		break;
	case AudioContext::DistanceModel_Linear:
		alModel = AL_LINEAR_DISTANCE;
		break;
	case AudioContext::DistanceModel_LinearClamped:
		alModel = AL_LINEAR_DISTANCE_CLAMPED;
		break;
	case AudioContext::DistanceModel_Exponential:
		alModel = AL_EXPONENT_DISTANCE;
		break;
	case AudioContext::DistanceModel_ExponentialClamped:
		alModel = AL_EXPONENT_DISTANCE_CLAMPED;
		break;
	default:
		Q_ASSERT_X( false,
			"Grim::AudioContextPrivate::setDistanceModel()",
			"Invalid distance model given." );
	}

	AudioContextLocker contextLocker = lock();
	alDistanceModel( alModel );
}


void AudioContextPrivate::setDopplerFactor( float factor )
{
	if ( dopplerFactor_ == factor )
		return;

	dopplerFactor_ = factor;

	AudioContextLocker contextLocker = lock();
	alDopplerFactor( dopplerFactor_ );
}


void AudioContextPrivate::setSpeedOfSound( float speed )
{
	if ( speedOfSound_ == speed )
		return;

	speedOfSound_ = speed;

	AudioContextLocker contextLocker = lock();
	alSpeedOfSound( speedOfSound_ );
}


AudioListener * AudioContextPrivate::defaultListener() const
{
	return defaultAudioListener_;
}


AudioListener * AudioContextPrivate::currentListener() const
{
	return currentAudioListener_;
}


AudioListener * AudioContextPrivate::createListener()
{
	AudioListener * audioListener = new AudioListener( new AudioListenerPrivate( audioContext_ ) );
	audioListeners_ << audioListener;
	return audioListener;
}


AudioSource * AudioContextPrivate::createSource()
{
	AudioSource * audioSource = new AudioSource( new AudioSourcePrivate( audioContext_ ) );
	audioSources_ << audioSource;
	return audioSource;
}


/**
 * Called from AudioDevicePrivate when AudioBufferLoader has finished buffer loading for the given \a audioSource.
 */

void AudioContextPrivate::addSourceForFinishedBuffer( AudioSource * audioSource )
{
	QWriteLocker finishedAudioSourcesLocker( &finishedAudioSourcesMutex_ );

	// only one request per source allowed
	Q_ASSERT( !finishedAudioSources_.contains( audioSource ) );

	finishedAudioSources_ << audioSource;

	if ( !finishedAudioSourcesPosted_ )
	{
		QCoreApplication::postEvent( this, new QEvent( (QEvent::Type)EventType_FinishedAudioSources ) );
		finishedAudioSourcesPosted_ = true;
	}
}


/**
 * Called from AudioBufferPrivate to ensure that source will not receive finished request for this buffer.
 */

void AudioContextPrivate::removeSourceForFinishedBuffer( AudioSource * audioSource )
{
	QWriteLocker finishedAudioSourcesLocker( &finishedAudioSourcesMutex_ );
	finishedAudioSources_.removeOne( audioSource );
}


void AudioContextPrivate::addActiveSource( AudioSource * audioSource )
{
	Q_ASSERT( !activeAudioSources_.contains( audioSource ) );
	activeAudioSources_ << audioSource;
	_updateActiveAudioSourcesTimer();
}


void AudioContextPrivate::removeActiveSource( AudioSource * audioSource )
{
	Q_ASSERT( activeAudioSources_.contains( audioSource ) );
	activeAudioSources_.removeOne( audioSource );
	_updateActiveAudioSourcesTimer();
}


bool AudioContextPrivate::event( QEvent * e )
{
	if ( e->type() == (QEvent::Type)EventType_FinishedAudioSources )
	{
		QWriteLocker finishedAudioSourcesLocker( &finishedAudioSourcesMutex_ );

		finishedAudioSourcesPosted_ = false;

		if ( !finishedAudioSources_.isEmpty() )
		{
			AudioContextLocker contextLocker = lock();

			for ( QListIterator<AudioSource*> it( finishedAudioSources_ ); it.hasNext(); )
			{
				AudioSource * audioSource = it.next();

				// When request->hasError == true we have routine that stops source,
				// this is normal policy, when buffer points to invalid data.
				// Stopping source means detaching buffer, and if detached buffer has no more
				// references -> it's destroyed. Together with buffer destruction it's mutex
				// will be destroyed too.
				// Ensure that audioBufferLocker (see bottom) unlocks this mutex BEFORE buffer destructor by
				// creating copy audioBufferCopy one pos upper in stack.

				AudioBuffer audioBufferCopy = audioSource->d_->audioBuffer_;

				{
					QWriteLocker audioBufferLocker( &audioSource->d_->audioBuffer_.d_->mutex_ );

					AudioBufferRequest * request = audioSource->d_->audioBuffer_.d_->requestForSource( audioSource );

					if ( !request->isProcessed )
					{
						request->isProcessed = true;
						if ( !request->hasError )
						{
							request->alBuffer = AudioManagerPrivate::sharedManagerPrivate()->createOpenALBuffer( request->content );
						}
					}

					if ( request->hasError )
					{
						// mark request as inactive, we will not process him further into source
						request->isActive = false;

						audioSource->d_->stopSelf();

						// do not detach buffer, leave this for user
						// audioSource->d_->deinitializeSelf();
					}
					else
					{
						if ( audioSource->d_->audioBuffer_.d_->isStreaming() )
						{
							audioSource->d_->setQueueOpenALBuffer( request );
						}
						else
						{
							audioSource->d_->setStaticOpenALBuffer( request );
						}
					}
				}
			}

			finishedAudioSources_.clear();
		}

		return true;
	}

	return QObject::event( e );
}


void AudioContextPrivate::timerEvent( QTimerEvent * e )
{
	if ( e->timerId() == activeAudioSourcesTimer_.timerId() )
	{
		_processActiveAudioSources();
		return;
	}

	return QObject::timerEvent( e );
}


void AudioContextPrivate::_updateActiveAudioSourcesTimer()
{
	if ( activeAudioSources_.isEmpty() || !isProcessing_ )
	{
		activeAudioSourcesTimer_.stop();
	}
	else
	{
		if ( !activeAudioSourcesTimer_.isActive() )
			activeAudioSourcesTimer_.start( ActiveAudioSourcesTimeout, this );
	}
}


void AudioContextPrivate::_processActiveAudioSources()
{
	AudioContextLocker contextLocker = lock();

	QList<AudioSource*> copy = activeAudioSources_;

	for ( QListIterator<AudioSource*> it( copy ); it.hasNext(); )
	{
		AudioSource * audioSource = it.next();

		Q_ASSERT( audioSource->d_->isActive_ );

		audioSource->d_->processSelf();
	}

	_updateActiveAudioSourcesTimer();
}




/**
 * \class AudioContext
 *
 * \ingroup audio_module
 *
 * \brief The AudioContext class represents isolated 3D audio environment.
 *
 * \reentrant
 *
 * Audio context is a single 3D scene.
 * Typical audio scene populated with number of AudioSource's and at least one AudioListener.
 *
 * Audio contexts are created with AudioDevice::createContext() call.
 *
 * After construction context has one default listener, that is set current.
 * You can create as many listeners as you want with createListener(), but only one can be current at time.
 * Use AudioListener::setCurrent() to switch them.
 *
 * After construction context is empty, use createSource() to populate is with sounds.
 *
 * Use can temporary mute and restore context processing with setEnabled() call.
 * Contexts are enabled by default after construction.
 *
 * Use createBuffer() to create audio buffers that can be passed to AudioSource::setBuffer() to play.
 *
 * Each context has it's own global policies about distance model and Doppler effect.
 * You can control them with setDistanceModel(), setDopplerFactor(), setSpeedOfSound().
 *
 * \openal
 */


/**
 * \enum AudioContext::DistanceModel
 *
 * This enum specified one of distance models that can be applied to the audio context environment.
 *
 * \openal
 */
/**\var AudioContext::DistanceModel AudioContext::DistanceModel_None
 * No attenuation calculations will be used.
 */
/**\var AudioContext::DistanceModel AudioContext::DistanceModel_Inverse
 * Refers to OpenAL Inverse Distance Attenuation Model.
 */
/**\var AudioContext::DistanceModel AudioContext::DistanceModel_InverseClamped
 * Refers to OpenAL Inverse Clamped Distance Attenuation Model.
 */
/**\var AudioContext::DistanceModel AudioContext::DistanceModel_Linear
 * Refers to OpenAL Linear Distance Attenuation Model.
 */
/**\var AudioContext::DistanceModel AudioContext::DistanceModel_LinearClamped
 * Refers to OpenAL Linear Clamped Distance Attenuation Model.
 */
/**\var AudioContext::DistanceModel AudioContext::DistanceModel_Exponential
 * Refers to OpenAL Exponential Distance Attenuation Model.
 */
/**\var AudioContext::DistanceModel AudioContext::DistanceModel_ExponentialClamped
 * Refers to OpenAL Exponential Clamped Distance Attenuation Model.
 */


/** \internal
 */

AudioContext::AudioContext( AudioContextPrivate * d ) :
	d_( d )
{
	d_->audioContext_ = this;

	d_->defaultAudioListener_ = d_->createListener();
	d_->defaultAudioListener_->setCurrent();
}


/**
 * Destroys context instance.
 * This also automatically destroys all created audio sources and audio listeners.
 */

AudioContext::~AudioContext()
{
	delete d_;
}


/**
 * \fn void AudioContext::sourceInitializationChanged( Grim::AudioSource * audioSource )
 *
 * This signal is emitted when \a audioSource's initialization state changes.
 */


/**
 * \fn void AudioContext::sourceStateChanged( Grim::AudioSource * audioSource )
 *
 * This signal is emitted when \a audioSource's state changed.
 */


/**
 * \fn void AudioContext::sourceCurrentOffsetChanged( Grim::AudioSource * audioSource )
 *
 * This signal is emitted when \a audioSource's playback position is changed.
 */


/**
 * Returns audio device this context belongs to.
 *
 * \sa AudioDevice::createContext()
 */

AudioDevice * AudioContext::device() const
{
	return d_->audioDevice_;
}


/**
 * Returns whether this context enabled or not.
 * Disabled context are completely muted and have internal state frozen.
 * By default contexts are enabled.
 *
 * \sa setEnabled()
 */

bool AudioContext::isEnabled() const
{
	return d_->isProcessing_;
}


/**
 * Set this context to be enabled if \a set is \c true or disabled if \a set is \c false.
 *
 * \sa isEnabled()
 */

void AudioContext::setEnabled( bool set )
{
	d_->setProcessing( set );
}


/**
 * Returns default listener instance.
 * This instance is always valid and will be used if current listener was destroyed explicitly.
 * You cannot destroy default listener.
 *
 * \sa createListener()
 */

AudioListener * AudioContext::defaultListener() const
{
	return d_->defaultListener();
}


/**
 * Returns current listener.
 *
 * \sa AudioListener::isCurrent()
 */

AudioListener * AudioContext::currentListener() const
{
	return d_->currentListener();
}


/**
 * Constructs and returns new listener instance.
 *
 * \sa AudioListener::setCurrent()
 */

AudioListener * AudioContext::createListener()
{
	return d_->createListener();
}


/**
 * Constructs and returns new source instance.
 */

AudioSource * AudioContext::createSource()
{
	return d_->createSource();
}


/**
 * Constructs and returns explicitly shared audio buffer instance with data source from the given
 * file with \a fileName.
 *
 * If \a format is specified explicitly it will be a mandatory to data decoding process.
 * Leave \a format null to allow to search suitable audio codec automatically.
 * List of available audio formats can be listed with AudioManager::availableFileFormats().
 *
 * This method does not blocks and returns immediately, real data decoding and buffer construction will be
 * done in separate thread instantly or by demand, depending on the given \a policy flags.
 * When buffer will be constructed sourceStateChanged() will be emitted for audio source for which this
 * buffer was constructed.
 *
 * \sa AudioManager::availableFileFormats()
 */

AudioBuffer AudioContext::createBuffer( const QString & fileName, const QByteArray & format, AudioBuffer::Policy policy )
{
	return d_->audioDevice_->d_->createBuffer( fileName, format, policy );
}


/**
 * Constructs and returns explicitly shared audio buffer instance from the given raw \a audioBufferData.
 * Returned buffer will be static, i.e. fully loaded into memory and will be unloaded by explicit destruction.
 */

AudioBuffer AudioContext::createBuffer( const AudioBufferData & audioBufferData )
{
	return d_->audioDevice_->d_->createBuffer( audioBufferData, this );
}


/**
 * Returns current distance model for this context that will be used in distance attenuation calculations.
 * Default distance model is DistanceModel_InverseClamped.
 *
 * \openal
 *
 * \sa setDistanceModel()
 */

AudioContext::DistanceModel AudioContext::distanceModel() const
{
	return d_->distanceModel();
}


/**
 * Set current distance model to be the given \a distanceModel.
 *
 * \openal
 *
 * \sa distanceModel()
 */

void AudioContext::setDistanceModel( DistanceModel distanceModel )
{
	d_->setDistanceModel( distanceModel );
}


/**
 * Returns current Doppler factor that will be used in Doppler effect calculations.
 * By default Doppler factor is 1.
 *
 * \openal
 *
 * \sa setDopplerFactor(), speedOfSound()
 */

float AudioContext::dopplerFactor() const
{
	return d_->dopplerFactor();
}


/**
 * Set Doppler factor to be the given \a factor.
 *
 * \openal
 *
 * \sa dopplerFactor(), setSpeedOfSound()
 */

void AudioContext::setDopplerFactor( float factor )
{
	d_->setDopplerFactor( factor );
}


/**
 * Returns current speed of sound value for this audio context, that will be user in Doppler effect calculations.
 * By default speed of sound is 343.3, which equals to speed of sound in air as propagation medium in meters per second.
 *
 * \openal
 *
 * \sa setSpeedOfSound(), dopplerFactor()
 */

float AudioContext::speedOfSound() const
{
	return d_->speedOfSound();
}


/**
 * Set speed of sound value to be the given \a speed.
 *
 * \openal
 *
 * \sa speedOfSound(), dopplerFactor()
 */

void AudioContext::setSpeedOfSound( float speed )
{
	d_->setSpeedOfSound( speed );
}




} // namespace Grim
