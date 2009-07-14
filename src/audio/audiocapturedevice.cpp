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

#include "audiocapturedevice.h"

#include "audio_p.h"
#include "audiobufferdata.h"




namespace Grim {




AudioCaptureDevicePrivate::AudioCaptureDevicePrivate( const QByteArray & name, ALCdevice * alcCaptureDevice,
	ALCenum alFormat, int channelsCount, int frequency, int bitsPerSample, int maxSamples ) :
	name_( name ), alcCaptureDevice_( alcCaptureDevice ), alFormat_( alFormat ),
	channelsCount_( channelsCount ),
	frequency_( frequency ),
	bitsPerSample_( bitsPerSample ),
	maxSamples_( maxSamples ),
	captureInterval_( 0 )
{
	bytesPerSample_ = channelsCount_ * (bitsPerSample_ >> 3);
}


AudioCaptureDevicePrivate::~AudioCaptureDevicePrivate()
{
	alcCaptureCloseDevice( alcCaptureDevice_ );
	alcCaptureDevice_ = 0;

	// remove self from manager
	AudioManagerPrivate::sharedManagerPrivate()->removeCaptureDevice( audioCaptureDevice_ );
}


void AudioCaptureDevicePrivate::setCaptureInterval( int interval )
{
	Q_ASSERT( interval >= 0 );

	captureInterval_ = interval;

	if ( captureInterval_ == 0 )
	{
		timer_.stop();
	}
	else
	{
		if ( timer_.isActive() )
			timer_.start( captureInterval_, this );
	}
}


AudioBufferData AudioCaptureDevicePrivate::captureBufferData()
{
	return _processSamples();
}


void AudioCaptureDevicePrivate::start()
{
	if ( timer_.isActive() )
		return;

	alcCaptureStart( alcCaptureDevice_ );

	timer_.start( captureInterval_, this );
}


void AudioCaptureDevicePrivate::stop()
{
	if ( !timer_.isActive() )
		return;

	timer_.stop();

	alcCaptureStop( alcCaptureDevice_ );
}


void AudioCaptureDevicePrivate::timerEvent( QTimerEvent * e )
{
	if ( e->timerId() == timer_.timerId() )
	{
		AudioBufferData audioBufferData = _processSamples();
		if ( !audioBufferData.isNull() )
			emit audioCaptureDevice_->bufferReady( audioBufferData );
		return;
	}

	QObject::timerEvent( e );
}


AudioBufferData AudioCaptureDevicePrivate::_processSamples()
{
	ALCint samplesCount;
	alcGetIntegerv( alcCaptureDevice_, ALC_CAPTURE_SAMPLES, 1, &samplesCount );

	if ( samplesCount == 0 )
		return AudioBufferData();

	QByteArray buffer;
	buffer.resize( samplesCount * bytesPerSample_ );

	alcCaptureSamples( alcCaptureDevice_, buffer.data(), samplesCount );

	return AudioBufferData( channelsCount_, bitsPerSample_, frequency_, samplesCount, buffer );
}




/**
 * \class AudioCaptureDevice
 *
 * \ingroup audio_module
 *
 * \brief The AudioCaptureDevice class records sound from audio capture devices.
 *
 * Capture device instances are constructed with AudioManager::createCaptureDevice().
 *
 * After construction device is created with limited size of internal buffer for sound samples.
 * When record is started you normally should take samples from buffer until it will not overflow.
 * Overflowing is not critical except that you can loose samples.
 *
 * There are two ways of gathering samples from capture device:
 *
 * 1. Request for samples from time to time with captureBufferData().
 * If buffer was empty - null AudioBufferData is returned.
 *
 * 2. Set check interval with setCaptureInterval(), after which bufferReady() signal will be emitted
 * with AudioBufferData instance as parameter.
 * By default interval is not set, which means that signal will be not emitted.
 *
 * \sa AudioManager::createCaptureDevice(), AudioBufferData
 */


/** \internal
 */

AudioCaptureDevice::AudioCaptureDevice( AudioCaptureDevicePrivate * d ) :
	d_( d )
{
	d_->audioCaptureDevice_ = this;
}


/**
 * Destroys capture device instance.
 */

AudioCaptureDevice::~AudioCaptureDevice()
{
	delete d_;
}


/**
 * \fn void AudioCaptureDevice::bufferReady( const Grim::AudioBufferData & audioBufferData )
 *
 * This signal is emitted automatically with each captureInterval() timeout with \a audioBufferData samples as parameter.
 * By default this signal will not be emitted, unless setCaptureInterval() methods will be called with positive interval timeout.
 *
 * \sa setCaptureInterval()
 */


/**
 * Returns number of channels of samples that this device gathers.
 */

int AudioCaptureDevice::channelsCount() const
{
	return d_->channelsCount_;
}


/**
 * Returns number of bits per sample of samples that this device gathers.
 */

int AudioCaptureDevice::bitsPerSample() const
{
	return d_->bitsPerSample_;
}


/**
 * Returns frequency of samples that this device gathers.
 */

int AudioCaptureDevice::frequency() const
{
	return d_->frequency_;
}


/**
 * Returns maximum number of samples that can be in internal buffer.
 */

int AudioCaptureDevice::maxSamples() const
{
	return d_->maxSamples_;
}


/**
 * Returns current internal with which device gathers samples.
 *
 * \sa setCaptureInterval()
 */

int AudioCaptureDevice::captureInterval() const
{
	return d_->captureInterval_;
}


/**
 * Set interval with which device will gather samples and emit bufferReady() signal.
 * If interval is \c 0 then signal emission will be disabled.
 *
 * \sa captureInterval()
 */

void AudioCaptureDevice::setCaptureInterval( int interval )
{
	d_->setCaptureInterval( interval );
}


/**
 * Takes and returns all currently available samples from buffer.
 */

AudioBufferData AudioCaptureDevice::captureBufferData()
{
	return d_->captureBufferData();
}


/**
 * Starts samples recording.
 *
 * \sa stop()
 */

void AudioCaptureDevice::start()
{
	d_->start();
}


/**
 * Stops samples recording.
 *
 * \sa start()
 */

void AudioCaptureDevice::stop()
{
	d_->stop();
}




} // namespace Grim
