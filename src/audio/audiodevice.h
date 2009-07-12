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

#include <QObject>




namespace Grim {




class AudioDevicePrivate;
class AudioContext;




class GRIM_AUDIO_EXPORT AudioDevice : public QObject
{
	Q_OBJECT

public:
	~AudioDevice();

	QByteArray name() const;

	AudioContext * createContext( int frequency = -1, int refreshInterval = -1, int sync = -1,
		int monoSources = -1, int stereoSources = -1 );

private:
	AudioDevice( AudioDevicePrivate * d );

private:
	AudioDevicePrivate * d_;

	friend class AudioManagerPrivate;
	friend class AudioBuffer;
	friend class AudioBufferPrivate;
	friend class AudioContext;
	friend class AudioContextPrivate;
	friend class AudioSourcePrivate;
};




} // namespace Grim
