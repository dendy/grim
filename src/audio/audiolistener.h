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

#pragma once

#include "audioglobal.h"

#include "audiomath.h"




namespace Grim {




class AudioListenerPrivate;
class AudioContext;




class GRIM_AUDIO_EXPORT AudioListener
{
public:
	~AudioListener();

	AudioContext * context() const;

	bool isCurrent() const;
	void setCurrent();

	float gain() const;
	void setGain( float gain );

	AudioVector position() const;
	void setPosition( const AudioVector & position );

	AudioVector velocity() const;
	void setVelocity( const AudioVector & velocity );

	AudioVector orientationAt() const;
	AudioVector orientationUp() const;
	void setOrientation( const AudioVector & at, const AudioVector & up );

private:
	AudioListener( AudioListenerPrivate * d );

private:
	AudioListenerPrivate * d_;

	friend class AudioListenerPrivate;
	friend class AudioContextPrivate;
};




} // namespace Grim
