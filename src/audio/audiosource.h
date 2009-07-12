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

#include "audiomath.h"




namespace Grim {




class AudioContext;
class AudioSourcePrivate;
class AudioBuffer;




class GRIM_AUDIO_EXPORT AudioSource
{
public:
	enum State
	{
		State_Idle = 0,
		State_Paused,
		State_Playing,
		State_Stopped
	};

	~AudioSource();

	AudioContext * context() const;

	bool areSignalsBlocked() const;
	bool setSignalsBlocked( bool set );

	State state() const;

	bool isInitialized() const;

	bool isSequential() const;

	int channelsCount() const;
	int bitsPerSample() const;
	float frequency() const;

	qint64 totalSamples() const;
	qint64 firstSampleOffset() const;
	qint64 lastSampleOffset() const;

	qint64 currentSampleOffset() const;
	void setCurrentSampleOffset( qint64 offset );

	AudioBuffer buffer() const;
	void setBuffer( const AudioBuffer & buffer );

	bool isLooping() const;
	void setLooping( bool set );

	float gain() const;
	void setGain( float gain );

	float minGain() const;
	void setMinGain( float gain );

	float maxGain() const;
	void setMaxGain( float gain );

	AudioVector position() const;
	void setPosition( const AudioVector & position );

	AudioVector velocity() const;
	void setVelocity( const AudioVector & velocity );

	bool isRelativeToListener() const;
	void setRelativeToListener( bool set );

	float pitch() const;
	void setPitch( float pitch );

	AudioVector direction() const;
	void setDirection( const AudioVector & direction );

	float innerConeAngle() const;
	void setInnerCoreAngle( float angle );

	float outerConeAngle() const;
	void setOuterConeAngle( float angle );

	float outerConeGain() const;
	void setOuterConeGain( float gain );

	float referenceDistance() const;
	void setReferenceDistance( float distance );

	float rolloffFactor() const;
	void setRolloffFactor( float factor );

	float maxDistance() const;
	void setMaxDistance( float distance );

	void play();
	void pause();
	void stop();

private:
	AudioSource( AudioSourcePrivate * d );

private:
	AudioSourcePrivate * d_;

	friend class AudioSourcePrivate;
	friend class AudioContextPrivate;
	friend class AudioDevicePrivate;
	friend class AudioBufferPrivate;
};




} // namespace Grim
