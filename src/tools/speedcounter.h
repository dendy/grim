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

#include "toolsglobal.h"

#include <QList>




namespace Grim {




template <typename T>
class SpeedCounter
{
public:
	SpeedCounter();

	double value() const;

	int timeout() const;
	void setTimeout( int timeout );

	void reset();

	void hit( T samples, int msecs );

private:
	void _calculateSpeed();

private:
	int timeout_;
	struct Chunk { T samples; int msecs; };
	QList<Chunk> timeoutChunks_;
	T currentSamples_;
	int currentMsecs_;
	double value_;
};




typedef SpeedCounter<qint64> IntSpeedCounter;
typedef SpeedCounter<double> RealSpeedCounter;




template <typename T>
inline SpeedCounter<T>::SpeedCounter() :
	timeout_( 0 ),
	currentSamples_( 0 ),
	currentMsecs_( 0 ),
	value_( 0 )
{
}


template <typename T>
inline double SpeedCounter<T>::value() const
{
	return value_;
}


template <typename T>
inline int SpeedCounter<T>::timeout() const
{
	return timeout_;
}


template <typename T>
inline void SpeedCounter<T>::setTimeout( int timeout )
{
	if ( timeout_ == timeout )
		return;

	timeout_ = timeout;

	_calculateSpeed();
}


template <typename T>
inline void SpeedCounter<T>::reset()
{
	timeoutChunks_.clear();
	currentSamples_ = T( 0 );
	currentMsecs_ = 0;
	value_ = 0;
}


template <typename T>
inline void SpeedCounter<T>::hit( T samples, int msecs )
{
	Q_ASSERT( msecs >= 0 );

	Chunk chunk;
	chunk.samples = samples;
	chunk.msecs = msecs;
	timeoutChunks_ << chunk;

	currentSamples_ += samples;
	currentMsecs_ += msecs;

	_calculateSpeed();
}



template <typename T>
inline void SpeedCounter<T>::_calculateSpeed()
{
	if ( timeoutChunks_.isEmpty() )
	{
		Q_ASSERT( value_ == 0 );
		return;
	}

	// clear extra chunks
	while ( currentMsecs_ > timeout_ )
	{
		// can't be less then one chunk
		if ( timeoutChunks_.count() == 1 )
			break;

		Chunk chunk = timeoutChunks_.takeFirst();
		currentSamples_ -= chunk.samples;
		currentMsecs_ -= chunk.msecs;
	}

	if ( currentMsecs_ == 0 )
	{
		// speed is infinite, but we will mark it as 0
		value_ = 0;
	}
	else
	{
		value_ = (double)currentSamples_ / currentMsecs_;
	}
}




} // namespace Grim
