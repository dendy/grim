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

#include "audiobufferdata.h"

#include <QByteArray>




namespace Grim {




class AudioBufferDataPrivate : public QSharedData
{
public:
	AudioBufferDataPrivate() :
		channelsCount( 0 ),
		bitsPerSample( 0 ),
		frequency( 0 ),
		samplesCount( 0 )
	{
	}

	void assertSelf()
	{
		Q_ASSERT( channelsCount == 1 || channelsCount == 2 );
		Q_ASSERT( bitsPerSample == 8 || bitsPerSample == 16 );
		Q_ASSERT( frequency > 0 );
		Q_ASSERT( samplesCount*channelsCount*(bitsPerSample>>3) == data.size() );
	}

	int channelsCount;
	int bitsPerSample;
	int frequency;
	int samplesCount;
	QByteArray data;
};




/**
 * \class AudioBufferData
 *
 * \ingroup audio_module
 *
 * \brief The AudioBufferData class holds raw sound samples.
 *
 * \threadsafe
 *
 * This class is implicitly shared, which means that you can safely transfer samples between audio contexts,
 * that works in different threads.
 *
 * \sa AudioCaptureDevice, AudioContext::createBuffer()
 */


/**
 * Constructs null buffer data.
 */

AudioBufferData::AudioBufferData()
{
}


/**
 * Constructs buffer data from the given raw \a data samples.
 * Additional information that describes data must be provided, it consists of:
 *
 * \a channelsCount  - number of channels of the given samples,
 * currently possible values are \c 1 for mono and \c 2 for stereo.
 *
 * \a bitsPerSample - number of bits per sample of the given samples,
 * currently possible values are \c 8 and \c 16 bits per sample.
 *
 * \a frequency of the given samples, which should be positive integer in Hz.
 *
 * \a samplesCount - number of samples, stored in \a data.
 *
 * Additionally constructor validates given values to ensure that data size equals information about samples:
 *
 * \a data.size() must equal \a channelsCount * \a (bitsPerSample/8) * \a samplesCount.
 *
 * If this is not met assert will be triggered.
 */

AudioBufferData::AudioBufferData( int channelsCount, int bitsPerSample, int frequency, int samplesCount, const QByteArray & data ) :
	d_( new AudioBufferDataPrivate )
{
	d_->channelsCount = channelsCount;
	d_->bitsPerSample = bitsPerSample;
	d_->frequency = frequency;
	d_->samplesCount = samplesCount;
	d_->data = data;

	d_->assertSelf();
}


/**
 * Constructs buffer data copy from the given \a audioBufferData.
 * This takes constant time because of implicitly sharing.
 */

AudioBufferData::AudioBufferData( const AudioBufferData & audioBufferData ) :
	d_( audioBufferData.d_ )
{
}


/**
 * Assigns this data to the given \a audioBufferData.
 * This takes constant time because of implicitly sharing.
 */

AudioBufferData & AudioBufferData::operator=( const AudioBufferData & audioBufferData )
{
	d_ = audioBufferData.d_;
	return *this;
}


/**
 * Destroys buffer data.
 */

AudioBufferData::~AudioBufferData()
{
}


/**
 * Returns whether this buffer data is null, i.e. was created with default constructor.
 */

bool AudioBufferData::isNull() const
{
	return !d_;
}


/**
 * Returns number of channels of the stored samples.
 */

int AudioBufferData::channelsCount() const
{
	return d_ ? d_->channelsCount : 0;
}


/**
 * Returns number of bits per sample of the stored samples.
 */

int AudioBufferData::bitsPerSample() const
{
	return d_ ? d_->bitsPerSample : 0;
}


/**
 * Returns frequency of the stored samples.
 */

int AudioBufferData::frequency() const
{
	return d_ ? d_->frequency : 0;
}


/**
 * Returns number of stored samples.
 */

int AudioBufferData::samplesCount() const
{
	return d_ ? d_->samplesCount : 0;
}


/**
 * Returns raw samples.
 */

QByteArray AudioBufferData::data() const
{
	return d_ ? d_->data : QByteArray();
}




} // namespace Grim
