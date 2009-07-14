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




namespace Grim {




class AudioBufferData;
class AudioCaptureDevicePrivate;




class GRIM_AUDIO_EXPORT AudioCaptureDevice : public QObject
{
	Q_OBJECT

	Q_PROPERTY( int captureInterval READ captureInterval WRITE setCaptureInterval )

public:
	~AudioCaptureDevice();

	int channelsCount() const;
	int bitsPerSample() const;
	int frequency() const;
	int maxSamples() const;

	int captureInterval() const;
	void setCaptureInterval( int interval );

	AudioBufferData captureBufferData();

public slots:
	void start();
	void stop();

signals:
	void bufferReady( const Grim::AudioBufferData & audioBufferData );

private:
	AudioCaptureDevice( AudioCaptureDevicePrivate * d );

	AudioCaptureDevicePrivate * d_;

	friend class AudioManagerPrivate;
	friend class AudioCaptureDevicePrivate;
};




} // namespace Grim
