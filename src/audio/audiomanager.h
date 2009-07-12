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




class AudioDevice;
class AudioCaptureDevice;
class AudioManagerPrivate;




class GRIM_AUDIO_EXPORT AudioManager : public QObject
{
	Q_OBJECT

public:
	static AudioManager * sharedManager();

	QList<QByteArray> availableFileFormats() const;
	QList<QString> availableFileFormatExtensions() const;

	QList<QByteArray> availableDeviceNames() const;
	QByteArray defaultDeviceName() const;

	QList<QByteArray> availableCaptureDeviceNames() const;
	QByteArray defaultCaptureDeviceName() const;

	QList<QByteArray> availableAllDeviceNames() const;
	QByteArray defaultAllDeviceName() const;

	AudioDevice * createDevice( const QByteArray & deviceName = QByteArray() );

	AudioCaptureDevice * createCaptureDevice( const QByteArray & captureDeviceName = QByteArray(),
		int channelsCount = 0, int bitsPerSamples = 0, int frequency = 0, int maxSamples = 0 );

private:
	AudioManager();
	~AudioManager();

	AudioManagerPrivate * d_;

	friend class AudioManagerPrivate;
};




} // namespace Grim
