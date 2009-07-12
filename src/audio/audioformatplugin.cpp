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

#include "audioformatplugin.h"




namespace Grim {




class AudioFormatFilePrivate
{
public:
	void setSampleChunkSize()
	{
		if ( channels == -1 || bitsPerSample == -1 )
			return;

		sampleChunkSize = channels * bytesPerSample;
	}

	// given
	QString fileName;
	QByteArray format;

	// resolved
	QByteArray resolvedFormat;
	int channels;
	int frequency;
	int bitsPerSample;
	qint64 totalSamples;

	// generated
	int bytesPerSample;
	int sampleChunkSize;
};




AudioFormatFile::AudioFormatFile( const QString & fileName, const QByteArray & format ) :
	d_( new AudioFormatFilePrivate )
{
	d_->fileName = fileName;
	d_->format = format;

	d_->channels = -1;
	d_->frequency = -1;
	d_->bitsPerSample = -1;
	d_->totalSamples = -1;

	d_->bytesPerSample = -1;
	d_->sampleChunkSize = -1;
}


AudioFormatFile::~AudioFormatFile()
{
	delete d_;
}


QString AudioFormatFile::fileName() const
{
	return d_->fileName;
}


QByteArray AudioFormatFile::format() const
{
	return d_->format;
}


QByteArray AudioFormatFile::resolvedFormat() const
{
	return d_->resolvedFormat;
}


int AudioFormatFile::channels() const
{
	return d_->channels;
}


int AudioFormatFile::frequency() const
{
	return d_->frequency;
}


int AudioFormatFile::bitsPerSample() const
{
	return d_->bitsPerSample;
}


qint64 AudioFormatFile::totalSamples() const
{
	return d_->totalSamples;
}


qint64 AudioFormatFile::truncatedSize( qint64 size ) const
{
	Q_ASSERT( d_->channels != -1 && d_->frequency != -1 && d_->bitsPerSample != -1 );

	const int oddSize = size % d_->sampleChunkSize;

	return size - oddSize;
}


qint64 AudioFormatFile::bytesToSamples( qint64 bytes ) const
{
	Q_ASSERT( d_->channels != -1 && d_->frequency != -1 && d_->bitsPerSample != -1 );
	Q_ASSERT( bytes >= 0 && (bytes % d_->sampleChunkSize) == 0 );

	return bytes / d_->sampleChunkSize;
}


qint64 AudioFormatFile::samplesToBytes( qint64 samples ) const
{
	Q_ASSERT( d_->channels != -1 && d_->frequency != -1 && d_->bitsPerSample != -1 );
	Q_ASSERT( samples >= 0 );

	return samples * d_->sampleChunkSize;
}


void AudioFormatFile::setResolvedFormat( const QByteArray & format )
{
	d_->resolvedFormat = format;
}


void AudioFormatFile::setChannels( int channels )
{
	Q_ASSERT( channels > 0 );

	d_->channels = channels;

	d_->setSampleChunkSize();
}


void AudioFormatFile::setFrequency( int frequency )
{
	Q_ASSERT( frequency >= 0 );

	d_->frequency = frequency;
}


void AudioFormatFile::setBitsPerSample( int bitsPerSample )
{
	Q_ASSERT( bitsPerSample >= 0 && (bitsPerSample & 0x07) == 0 );

	d_->bitsPerSample = bitsPerSample;
	d_->bytesPerSample = bitsPerSample >> 3;

	d_->setSampleChunkSize();
}


void AudioFormatFile::setTotalSamples( qint64 totalSamples )
{
	Q_ASSERT( totalSamples >= 0 || totalSamples == -1 );

	d_->totalSamples = totalSamples;
}




} // namespace Grim
