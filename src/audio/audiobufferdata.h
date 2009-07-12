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

#pragma once

#include "audioglobal.h"

#include <QSharedDataPointer>




class QByteArray;




namespace Grim {




class AudioBufferDataPrivate;




class GRIM_AUDIO_EXPORT AudioBufferData
{
public:
	AudioBufferData();
	AudioBufferData( int channelsCount, int bitsPerSample, int frequency, int samplesCount, const QByteArray & data );
	AudioBufferData( const AudioBufferData & audioBufferData );
	AudioBufferData & operator=( const AudioBufferData & audioBufferData );
	~AudioBufferData();

	int channelsCount() const;
	int bitsPerSample() const;
	int frequency() const;
	int samplesCount() const;
	QByteArray data() const;

	bool isNull() const;

private:
	QSharedDataPointer<AudioBufferDataPrivate> d_;
};




} // namespace Grim