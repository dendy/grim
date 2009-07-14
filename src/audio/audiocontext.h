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

#include <QObject>

#include "audiobuffer.h"




namespace Grim {




class AudioDevice;
class AudioContextPrivate;
class AudioListener;
class AudioSource;
class AudioBufferData;




class GRIM_AUDIO_EXPORT AudioContext : public QObject
{
	Q_OBJECT

public:
	enum DistanceModel
	{
		DistanceModel_None               = 0,
		DistanceModel_Inverse            = 1,
		DistanceModel_InverseClamped     = 2,
		DistanceModel_Linear             = 3,
		DistanceModel_LinearClamped      = 4,
		DistanceModel_Exponential        = 5,
		DistanceModel_ExponentialClamped = 6
	};

	Q_PROPERTY( bool isEnabled READ isEnabled WRITE setEnabled )

	~AudioContext();

	AudioDevice * device() const;

	bool isEnabled() const;
	void setEnabled( bool set );

	AudioListener * defaultListener() const;
	AudioListener * currentListener() const;

	AudioListener * createListener();

	AudioSource * createSource();

	AudioBuffer createBuffer( const QString & fileName, const QByteArray & format = QByteArray(),
		AudioBuffer::Policy policy = AudioBuffer::NoPolicy );

	AudioBuffer createBuffer( const AudioBufferData & audioBufferData );

	DistanceModel distanceModel() const;
	void setDistanceModel( DistanceModel distanceModel );

	float dopplerFactor() const;
	void setDopplerFactor( float factor );

	float speedOfSound() const;
	void setSpeedOfSound( float speed );

signals:
	void sourceInitializationChanged( Grim::AudioSource * audioSource );
	void sourceStateChanged( Grim::AudioSource * audioSource );
	void sourceCurrentOffsetChanged( Grim::AudioSource * audioSource );

private:
	AudioContext( AudioContextPrivate * d );

private:
	AudioContextPrivate * d_;

	friend class AudioDevicePrivate;
	friend class AudioManagerPrivate;
	friend class AudioListenerPrivate;
	friend class AudioBufferPrivate;
	friend class AudioSourcePrivate;
};




} // namespace Grim
