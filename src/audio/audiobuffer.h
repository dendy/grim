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

#include <QMetaType>
#include <QExplicitlySharedDataPointer>




namespace Grim {




class AudioBufferPrivate;




class GRIM_AUDIO_EXPORT AudioBuffer
{
public:
	enum PolicyFlag
	{
		NoPolicy      = 0x0000,
		// load policy
		LoadOnDemand  = 0x0001,
		// unload policy
		AutoUnload    = 0x0002,
		// streaming policy
		Streaming     = 0x0004
	};
	Q_DECLARE_FLAGS( Policy, PolicyFlag )

	AudioBuffer();
	AudioBuffer( const AudioBuffer & audioBuffer );
	AudioBuffer & operator=( const AudioBuffer & audioBuffer );

	~AudioBuffer();

	bool operator==( const AudioBuffer & audioBuffer ) const;

	QString fileName() const;
	QByteArray format() const;
	Policy policy() const;

private:
	AudioBuffer( AudioBufferPrivate * d );
	AudioBuffer & operator=( AudioBufferPrivate * d );

private:
	QExplicitlySharedDataPointer<AudioBufferPrivate> d_;

	friend class AudioBufferPrivate;
	friend class AudioDevicePrivate;
	friend class AudioBufferLoader;
	friend class AudioContextPrivate;
	friend class AudioSourcePrivate;
};




inline bool AudioBuffer::operator==( const AudioBuffer & audioBuffer ) const
{ return d_ == audioBuffer.d_; }




} // namespace Grim




Q_DECLARE_METATYPE( Grim::AudioBuffer )
