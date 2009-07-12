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

#include <FLAC/stream_decoder.h>
#include <FLAC/metadata.h>




namespace Grim {




class AudioFlacFormatFile;




class AudioFlacFormatDevice : public QIODevice
{
public:
	static FLAC__StreamDecoderReadStatus flac_read( const FLAC__StreamDecoder * decoder,
		FLAC__byte buffer[], size_t * bytes, void * client_data );
	static FLAC__StreamDecoderSeekStatus flac_seek( const FLAC__StreamDecoder * decoder,
		FLAC__uint64 absolute_byte_offset, void * client_data );
	static FLAC__StreamDecoderTellStatus flac_tell( const FLAC__StreamDecoder * decoder,
		FLAC__uint64 * absolute_byte_offset, void * client_data );
	static FLAC__StreamDecoderLengthStatus flac_length( const FLAC__StreamDecoder * decoder,
		FLAC__uint64 * stream_length, void * client_data );
	static FLAC__bool flac_eof( const FLAC__StreamDecoder * decoder,
		void * client_data );
	static FLAC__StreamDecoderWriteStatus flac_write( const FLAC__StreamDecoder * decoder,
		const FLAC__Frame * frame, const FLAC__int32 * const buffer[], void * client_data );
	static void flac_metadata( const FLAC__StreamDecoder * decoder,
		const FLAC__StreamMetadata * metadata, void * client_data );
	static void flac_error( const FLAC__StreamDecoder * decoder,
		FLAC__StreamDecoderErrorStatus status, void * client_data );

	AudioFlacFormatDevice( AudioFlacFormatFile * formatFile );
	~AudioFlacFormatDevice();

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

	void _processFrame( const FLAC__Frame * frame, const FLAC__int32 * const buffer[], void * data, qint64 samplesCount );

private:
	AudioFlacFormatFile * formatFile_;

	QFile file_;

	bool isSequential_;

	qint64 pos_;
	qint64 outputSize_;

	FLAC__StreamDecoder * flacDecoder_;

	// for metadata callback
	bool isStreamMetadataProcessed_;

	// for write callback
	bool isSeeking_;          // indicates whether we are inside seek or read

	char * readData_;         // input data pointer
	qint64 readMaxSize_;      // input data max size
	qint64 readedBytes_;      // output bytes count
	QByteArray readCache_;    // cache between readData() calls to store unhandled samples
	qint64 readedTotalBytes_; // total bytes readed so far at current readData() call

	qint64 seekSample_;       // sample that was passed to seek

	friend class DecoderDestroyer;
};




class AudioFlacFormatFile : public AudioFormatFile
{
public:
	AudioFlacFormatFile( const QString & fileName, const QByteArray & format );

	QIODevice * device();

private:
	AudioFlacFormatDevice device_;

	friend class AudioFlacFormatDevice;
};




class AudioFlacFormatPlugin : public QObject, public AudioFormatPlugin
{
	Q_OBJECT
	Q_INTERFACES( Grim::AudioFormatPlugin )

public:
	QList<QByteArray> formats() const;
	QStringList extensions() const;

	AudioFormatFile * createFile( const QString & fileName, const QByteArray & format );
};




} // namespace Grim
