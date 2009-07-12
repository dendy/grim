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

#include <grim/audio/audioformatplugin.h>

#include <QIODevice>
#include <QFile>




namespace Grim {




class AudioWaveFormatFile;




class AudioWaveFormatDevice : public QIODevice
{
public:
	typedef void (*Codec)( const void * data, int size, void * outData );

	AudioWaveFormatDevice( AudioWaveFormatFile * formatFile );
	~AudioWaveFormatDevice();

	bool open( QIODevice::OpenMode openMode );
	void close();

	bool isSequential() const;

	qint64 pos() const;
	bool atEnd() const;
	qint64 size() const;
	qint64 bytesAvailable() const;

	qint64 readData( char * data, qint64 maxSize );
	qint64 writeData( const char * data, qint64 maxSize );

	bool seek( qint64 pos );

private:
	int _codecMultiplier() const;

	bool _guessWave();
	bool _guessAu();

	bool _openRaw();
	bool _openWave();
	bool _openAu();

	void _setOutputSize();

	qint64 _readData( char * data, qint64 maxSize );

	bool _open( const QByteArray & format, bool firstTime );
	void _close();

private:
	AudioWaveFormatFile * formatFile_;

	QFile file_;

	bool isSequential_;

	bool isWave_;
	bool isAu_;

	Codec codec_;
	qint64 dataPos_;
	qint64 inputSize_;
	qint64 outputSize_;

	qint64 pos_;
};




class AudioWaveFormatFile : public AudioFormatFile
{
public:
	AudioWaveFormatFile( const QString & fileName, const QByteArray & format );

	QIODevice * device();

private:
	AudioWaveFormatDevice device_;

	friend class AudioWaveFormatDevice;
};




class AudioWaveFormatPlugin : public QObject, AudioFormatPlugin
{
	Q_OBJECT
	Q_INTERFACES( Grim::AudioFormatPlugin )

public:
	QList<QByteArray> formats() const;
	QStringList extensions() const;

	AudioFormatFile * createFile( const QString & fileName, const QByteArray & format );
};




} // namespace Grim
