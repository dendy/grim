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

#include <vorbis/vorbisfile.h>




namespace Grim {




class AudioVorbisFormatFile;




class AudioVorbisFormatDevice : public QIODevice
{
public:
	static size_t ogg_read( void * ptr, size_t size, size_t nmemb, void * datasource );
	static int ogg_seek( void * datasource, ogg_int64_t offset, int whence );
	static int ogg_close( void * datasource );
	static long ogg_tell( void * datasource );

	AudioVorbisFormatDevice( AudioVorbisFormatFile * formatFile );
	~AudioVorbisFormatDevice();

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
	bool _open( const QByteArray & format, bool firstTime );
	void _close();

private:
	AudioVorbisFormatFile * formatFile_;

	QFile file_;

	bool isSequential_;

	qint64 pos_;
	qint64 outputSize_;
	bool readError_;
	bool atEnd_;

	OggVorbis_File oggVorbisFile_;
};




class AudioVorbisFormatFile : public AudioFormatFile
{
public:
	AudioVorbisFormatFile( const QString & fileName, const QByteArray & format );

	QIODevice * device();

private:
	AudioVorbisFormatDevice device_;

	friend class AudioVorbisFormatDevice;
};




class AudioVorbisFormatPlugin : public QObject, public AudioFormatPlugin
{
	Q_OBJECT
	Q_INTERFACES( Grim::AudioFormatPlugin )

public:
	QList<QByteArray> formats() const;
	QStringList extensions() const;

	AudioFormatFile * createFile( const QString & fileName, const QByteArray & format );
};




} // namespace Grim
