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

#include "fpscounter.h"

#include "speedcounter.h"




namespace Grim {




static const int DefaultTimeout = 1000;



/** \internal
 * \class FpsCounterPrivate
 */

class FpsCounterPrivate
{
public:
	IntSpeedCounter counter_;
	QTime time_;
};




/** \internal
 *
 * Calculates FPS value and return whether it changed.
 * Also this will clean extra timeout chunks while currentTimeout_ value is bigger then timeout().
 */





/**
 * \class FpsCounter
 *
 * \ingroup tools_module
 *
 * \brief The FpsCounter class calculates 'Frame Per Second' value.
 *
 * FpsCounter calculates value that represents approximate performance for application, usually GUI thread.
 * To find FPS value for a certain period of time - create FpsCounter instance and call hit() once per cycle's iteration.
 */


/**
 * Creates FpsCounter instance with timeout value of 1 second. \a parent is passed to QObject constructor.
 */

FpsCounter::FpsCounter( QObject * parent ) :
	QObject( parent ), d_( new FpsCounterPrivate )
{
	d_->time_ = QTime::currentTime();
	d_->counter_.setTimeout( DefaultTimeout );
}


/**
 * Destroys FpsCounter instance.
 */

FpsCounter::~FpsCounter()
{
	delete d_;
}


/** \fn void FpsCounter::valueChanged( float value )
 *
 * This signal emitted every time FPS value changes with current \a value as argument.
 */


/**
 * \property FpsCounter::value
 *
 * Returns current FPS value which is always for the last timeout() milliseconds.
 */

float FpsCounter::value() const
{
	// we count FPS in frames per millisecond, but return in frames per second
	return (float)(d_->counter_.value() * 1000);
}


/**
 * Returns current timeout value in milliseconds. FPS value will be calculated for the last timeout range.
 *
 * \sa setTimeout()
 */

int FpsCounter::timeout() const
{
	return d_->counter_.timeout();
}


/**
 * Changes timeout value to the given \a timeout.
 *
 * \sa timeout()
 */

void FpsCounter::setTimeout( int timeout )
{
	qint64 previousValue = d_->counter_.value();

	d_->counter_.setTimeout( timeout );

	if ( d_->counter_.value() != previousValue )
		emit valueChanged( value() );
}


/**
 * Clears counter state for the called earlier hit() and resets FPS value.
 * Call this to restart FPS counting from zero.
 */

void FpsCounter::reset()
{
	d_->time_.start();

	qint64 previousValue = d_->counter_.value();

	d_->counter_.reset();

	if ( d_->counter_.value() != previousValue )
		emit valueChanged( value() );
}


/**
 * Advances counting and returns current FPS value. Also emmits valueChanged().
 * This should be called once per each frame in your application for which FPS should be counted.
 * The \a msecs is a delay between this frame and previous frame.
 * If not set manually - delay time will be calculated from your system time since last hit() call.
 *
 * \sa valueChanged()
 */

float FpsCounter::hit( int msecs )
{
	if ( msecs == -1 )
		msecs = d_->time_.restart();

	if ( msecs < 0 )
		return value();

	qint64 previousValue = d_->counter_.value();

	d_->counter_.hit( 1, msecs );

	if ( d_->counter_.value() != previousValue )
		emit valueChanged( value() );

	return value();
}




} // namespace Grim
