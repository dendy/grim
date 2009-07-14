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

#include "audioflacformatplugin.h"

#include <QStringList>
#include <QDebug>




namespace Grim {




static const QByteArray FlacFormatName = "FLAC";
static const QByteArray OggFlacFormatName = "Ogg/FLAC";




inline static AudioFlacFormatDevice * _takeFlacDevice( void * client_data )
{
	return static_cast<AudioFlacFormatDevice*>( client_data );
}


FLAC__StreamDecoderReadStatus AudioFlacFormatDevice::flac_read( const FLAC__StreamDecoder * decoder,
	FLAC__byte buffer[], size_t * bytes, void * client_data )
{
	QIODevice * device = &_takeFlacDevice( client_data )->file_;

	const qint64 bytesReaded = device->read( (char*)buffer, *bytes );
	if ( bytesReaded == -1 )
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

	*bytes = bytesReaded;

	if ( bytesReaded == 0 )
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}


FLAC__StreamDecoderSeekStatus AudioFlacFormatDevice::flac_seek( const FLAC__StreamDecoder * decoder,
	FLAC__uint64 absolute_byte_offset, void * client_data )
{
	QIODevice * device = &_takeFlacDevice( client_data )->file_;

	if ( device->isSequential() )
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;

	const bool done = device->seek( (qint64)absolute_byte_offset );

	if ( !done )
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}


FLAC__StreamDecoderTellStatus AudioFlacFormatDevice::flac_tell( const FLAC__StreamDecoder * decoder,
	FLAC__uint64 * absolute_byte_offset, void * client_data )
{
	QIODevice * device = &_takeFlacDevice( client_data )->file_;

	if ( device->isSequential() )
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;

	const qint64 pos = device->pos();

	if ( pos < 0 )
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

	*absolute_byte_offset = (FLAC__uint64)pos;

	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}


FLAC__StreamDecoderLengthStatus AudioFlacFormatDevice::flac_length( const FLAC__StreamDecoder * decoder,
	FLAC__uint64 * stream_length, void * client_data )
{
	QIODevice * device = &_takeFlacDevice( client_data )->file_;

	if ( device->isSequential() )
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;

	const qint64 size = device->size();

	if ( size < 0 )
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

	*stream_length = (FLAC__uint64)size;

	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}


FLAC__bool AudioFlacFormatDevice::flac_eof( const FLAC__StreamDecoder * decoder,
	void * client_data )
{
	QIODevice * device = &_takeFlacDevice( client_data )->file_;

	return (FLAC__bool)device->atEnd();
}


template<int channels, typename B>
static void _processFrameWorker( const FLAC__Frame * frame, const FLAC__int32 * const buffer[],
	void * data, qint64 samplesCount )
{
	B * b = (B*)data;
	for ( int i = 0; i < samplesCount; ++i )
	{
		b[ i*channels + 0 ] = buffer[ 0 ][ i ];
		if ( channels > 1 )
			b[ i*channels + 1 ] = buffer[ 1 ][ i ];
	}
}


void AudioFlacFormatDevice::_processFrame( const FLAC__Frame * frame, const FLAC__int32 * const buffer[],
	void * data, qint64 samplesCount )
{
	if ( formatFile_->channels() == 1 )
	{
		if ( formatFile_->bitsPerSample() == 8 )
			_processFrameWorker<1,qint8>( frame, buffer, data, samplesCount );
		else // if ( formatFile_->bitsPerSample() == 16 )
			_processFrameWorker<1,qint16>( frame, buffer, data, samplesCount );
	}
	else
	{
		if ( formatFile_->bitsPerSample() == 8 )
			_processFrameWorker<2,qint8>( frame, buffer, data, samplesCount );
		else // if ( formatFile_->bitsPerSample() == 16 )
			_processFrameWorker<2,qint16>( frame, buffer, data, samplesCount );
	}
}


FLAC__StreamDecoderWriteStatus AudioFlacFormatDevice::flac_write( const FLAC__StreamDecoder * decoder,
	const FLAC__Frame * frame, const FLAC__int32 * const buffer[], void * client_data )
{
	AudioFlacFormatDevice * flacDevice = _takeFlacDevice( client_data );

	const qint64 currentSample = flacDevice->isSeeking_ ?
		flacDevice->seekSample_ :
		flacDevice->formatFile_->bytesToSamples( flacDevice->pos_ + flacDevice->readedTotalBytes_ );

	if ( frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER )
	{
		if ( (qint64)frame->header.number.sample_number != currentSample )
		{
			// desynchronization after seeking happened
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
	}

	if ( flacDevice->formatFile_->channels() != (int)frame->header.channels )
	{
		// should not happen
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	qint64 remainSamples = frame->header.blocksize;
	qint64 samplesToReadToBuffer = 0;

	if ( !flacDevice->isSeeking_ )
	{
		// first write decoded samples into read buffer, if we in read mode

		const qint64 readMaxSamples = flacDevice->formatFile_->bytesToSamples( flacDevice->readMaxSize_ );

		samplesToReadToBuffer = qMin<qint64>( readMaxSamples, remainSamples );
		const qint64 bytesToReadToBuffer = flacDevice->formatFile_->samplesToBytes( samplesToReadToBuffer );

		flacDevice->_processFrame( frame, buffer, flacDevice->readData_, samplesToReadToBuffer );
		flacDevice->readedBytes_ = bytesToReadToBuffer;
	}

	// then write remaining samples into cache
	remainSamples -= samplesToReadToBuffer;

	// at this point cache is always cleared either from read or seek
	Q_ASSERT( flacDevice->readCache_.isEmpty() );

	if ( remainSamples > 0 )
	{
		flacDevice->readCache_.resize( flacDevice->formatFile_->samplesToBytes( remainSamples ) );

		// create temporary buffer with shifted sample offsets
		const FLAC__int32 * shiftedBuffer[ 2 ]; // 2 == maximum number of channels, this value is checked in open()
		for ( int i = 0; i < flacDevice->formatFile_->channels(); ++i )
			shiftedBuffer[ i ] = buffer[ i ] + samplesToReadToBuffer;

		flacDevice->_processFrame( frame, shiftedBuffer, flacDevice->readCache_.data(), remainSamples );
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void AudioFlacFormatDevice::flac_metadata( const FLAC__StreamDecoder * decoder,
	const FLAC__StreamMetadata * metadata, void * client_data )
{
	AudioFlacFormatDevice * flacDevice = _takeFlacDevice( client_data );

	if ( metadata->type == FLAC__METADATA_TYPE_STREAMINFO )
	{
		// can process only mono or stereo
		if ( metadata->data.stream_info.channels != 1 && metadata->data.stream_info.channels != 2 )
			return;

		// can process only 8 or 16 bits per sample
		if ( metadata->data.stream_info.bits_per_sample != 8 && metadata->data.stream_info.bits_per_sample != 16 )
			return;

		flacDevice->formatFile_->setResolvedFormat( FlacFormatName );
		flacDevice->formatFile_->setChannels( metadata->data.stream_info.channels );
		flacDevice->formatFile_->setFrequency( metadata->data.stream_info.sample_rate );
		flacDevice->formatFile_->setBitsPerSample( metadata->data.stream_info.bits_per_sample );
		flacDevice->formatFile_->setTotalSamples( metadata->data.stream_info.total_samples );

		flacDevice->isStreamMetadataProcessed_ = true;

		return;
	}
}


void AudioFlacFormatDevice::flac_error( const FLAC__StreamDecoder * decoder,
	FLAC__StreamDecoderErrorStatus status, void * client_data )
{
#ifdef GRIM_AUDIO_DEBUG
	qDebug() << "AudioFlacFormatDevice::flac_error() << Error status =" << status;
#endif
}




AudioFlacFormatDevice::AudioFlacFormatDevice( AudioFlacFormatFile * formatFile ) :
	formatFile_( formatFile )
{
	flacDecoder_ = 0;
	pos_ = -1;
	isSeeking_ = false;
}


AudioFlacFormatDevice::~AudioFlacFormatDevice()
{
	close();
}


bool AudioFlacFormatDevice::_open( const QByteArray & format, bool firstTime )
{
	bool previousSequential;
	qint64 previousChannels;
	qint64 previousBitsPerSample;
	qint64 previousFrequency;
	qint64 previousTotalSamples;

	if ( !firstTime )
	{
		previousSequential = isSequential_;
		previousChannels = formatFile_->channels();
		previousBitsPerSample = formatFile_->bitsPerSample();
		previousFrequency = formatFile_->frequency();
		previousTotalSamples = formatFile_->totalSamples();
	}

	isSequential_ = file_.isSequential();

	if ( !firstTime && isSequential_ != previousSequential )
		return false;

	flacDecoder_ = FLAC__stream_decoder_new();
	if ( !flacDecoder_ )
	{
		file_.close();
		return false;
	}

	FLAC__StreamDecoderInitStatus initStatus = isSequential_ ?
		FLAC__stream_decoder_init_stream( flacDecoder_,
			flac_read,
			0, // flac_seek
			0, // flac_tell
			0, // flac_length
			flac_eof,
			flac_write,
			flac_metadata,
			flac_error,
			this ) :
		FLAC__stream_decoder_init_stream( flacDecoder_,
			flac_read,
			flac_seek,
			flac_tell,
			flac_length,
			flac_eof,
			flac_write,
			flac_metadata,
			flac_error,
			this );

	if ( initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK )
	{
		FLAC__stream_decoder_delete( flacDecoder_ );
		flacDecoder_ = 0;
		file_.close();
		return false;
	}

	isStreamMetadataProcessed_ = false;

	const bool isProcessed = FLAC__stream_decoder_process_until_end_of_metadata( flacDecoder_ );
	if ( !isProcessed || !isStreamMetadataProcessed_ )
	{
		FLAC__stream_decoder_delete( flacDecoder_ );
		flacDecoder_ = 0;
		file_.close();
		return false;
	}

	if ( !firstTime )
	{
		if ( formatFile_->channels() != previousChannels ||
			formatFile_->bitsPerSample() != previousBitsPerSample ||
			formatFile_->frequency() != previousFrequency ||
			formatFile_->totalSamples() != previousTotalSamples )
		{
			FLAC__stream_decoder_delete( flacDecoder_ );
			flacDecoder_ = 0;
			file_.close();
			return false;
		}
	}

	outputSize_ = formatFile_->samplesToBytes( formatFile_->totalSamples() );
	pos_ = 0;

	return true;
}


bool AudioFlacFormatDevice::open( QIODevice::OpenMode openMode )
{
	Q_ASSERT( !isOpen() );

	if ( openMode != QIODevice::ReadOnly )
		return false;

	file_.setFileName( formatFile_->fileName() );

	if ( !file_.open( QIODevice::ReadOnly ) )
		return false;

	if ( !_open( formatFile_->format(), true ) )
		return false;

	setOpenMode( openMode );

	return true;
}


void AudioFlacFormatDevice::_close()
{
	if ( pos_ == -1 )
		return;

	FLAC__stream_decoder_delete( flacDecoder_ );
	flacDecoder_ = 0;

	readCache_ = QByteArray();

	pos_ = -1;
}


void AudioFlacFormatDevice::close()
{
	if ( !isOpen() )
		return;

	QIODevice::close();

	_close();

	file_.close();
}


bool AudioFlacFormatDevice::isSequential() const
{
	Q_ASSERT( isOpen() );

	return isSequential_;
}


qint64 AudioFlacFormatDevice::pos() const
{
	Q_ASSERT( isOpen() );

	return pos_;
}


bool AudioFlacFormatDevice::atEnd() const
{
	Q_ASSERT( isOpen() );

	return pos_ == outputSize_;
}


qint64 AudioFlacFormatDevice::size() const
{
	if ( !isOpen() )
		return 0;

	return outputSize_;
}


qint64 AudioFlacFormatDevice::bytesAvailable() const
{
	if ( isSequential_ )
		return 0 + QIODevice::bytesAvailable();
	else
		return outputSize_ - pos_;
}


qint64 AudioFlacFormatDevice::readData( char * data, qint64 maxSize )
{
	Q_ASSERT( isOpen() );

	isSeeking_ = false;

	readedTotalBytes_ = 0;

	readData_ = data;
	readMaxSize_ = qMin( formatFile_->truncatedSize( maxSize ), outputSize_ - pos_ );

	if ( readMaxSize_ == 0 )
		return readedTotalBytes_;

	// first fill data with samples from cache
	if ( !readCache_.isEmpty() )
	{
		const qint64 bytesToCopy = qMin<qint64>( readCache_.size(), readMaxSize_ );

		memcpy( readData_, readCache_.constData(), bytesToCopy );
		readCache_ = readCache_.mid( bytesToCopy );

		readData_ += bytesToCopy;
		readMaxSize_ -= bytesToCopy;
		readedTotalBytes_ += bytesToCopy;
	}

	if ( readMaxSize_ == 0 )
	{
		if ( pos_ + readedTotalBytes_ == outputSize_ )
		{
			Q_ASSERT( readCache_.isEmpty() );
		}
		return readedTotalBytes_;
	}

	// load rest bytes from decoder
	while ( readMaxSize_ > 0 )
	{
		readedBytes_ = -1;

		if ( !FLAC__stream_decoder_process_single( flacDecoder_ ) )
		{
			readCache_ = QByteArray();
			return -1;
		}

		if ( readedBytes_ == -1 )
		{
			Q_ASSERT( pos_ + readedTotalBytes_ == outputSize_ );
			break;
		}

		readData_ += readedBytes_;
		readMaxSize_ -= readedBytes_;
		readedTotalBytes_ += readedBytes_;
	}

	pos_ += readedTotalBytes_;

	qint64 tmp = readedTotalBytes_;
	readedTotalBytes_ = 0;
	return tmp;
}


qint64 AudioFlacFormatDevice::writeData( const char * data, qint64 maxSize )
{
	return -1;
}


bool AudioFlacFormatDevice::seek( qint64 pos )
{
	Q_ASSERT( isOpen() );

	QIODevice::seek( pos );

	if ( pos < 0 )
		return false;

	if ( pos == pos_ )
		return true;

	if ( isSequential_ )
	{
		if ( pos != 0 || (outputSize_ != -1 && pos > outputSize_) )
			return false;

		_close();

		const QIODevice::OpenMode openMode = file_.openMode();
		if ( !file_.reset() )
		{
			// seeking to 0 byte is not implemented
			// close and reopen file to archive the same effect
			file_.close();
			if ( !file_.open( openMode ) )
				return false;
		}

		return _open( formatFile_->resolvedFormat(), false );
	}
	else
	{
		if ( pos > outputSize_ )
			return false;
	}

	// Same as in Vorbis plugin.
	// Currently there are no routines to reach here in sequential mode.
	// If somebody wants to seek thru the FLAC stream in sequential mode
	// this would be a complex task with internal buffers and hard decoding
	// sequentially thru the whole stream.
	// The better idea is to operate on non compressed random-access files,
	// like ZIP-archive provides together with compressed ones in the same archive.
	Q_ASSERT( !isSequential_ );

	readedTotalBytes_ = 0;
	readCache_ = QByteArray();

	// this also asserts correct pos value
	seekSample_ = formatFile_->bytesToSamples( pos );

	isSeeking_ = true;

	const bool done = FLAC__stream_decoder_seek_absolute( flacDecoder_, seekSample_ );

	if ( !done )
		return false;

	pos_ = pos;

	return true;
}




AudioFlacFormatFile::AudioFlacFormatFile( const QString & fileName, const QByteArray & format ) :
	AudioFormatFile( fileName, format ), device_( this )
{
}


QIODevice * AudioFlacFormatFile::device()
{
	return &device_;
}




QList<QByteArray> AudioFlacFormatPlugin::formats() const
{
	return QList<QByteArray>() << FlacFormatName;
}


QStringList AudioFlacFormatPlugin::extensions() const
{
	return QStringList()
		<< QLatin1String( "flac" );
}


AudioFormatFile * AudioFlacFormatPlugin::createFile( const QString & fileName, const QByteArray & format )
{
	if ( !format.isNull() )
		Q_ASSERT( formats().contains( format ) );

	return new AudioFlacFormatFile( fileName, format );
}




} // namespace Grim




Q_EXPORT_PLUGIN2( grim_audio_flac_format_plugin, Grim::AudioFlacFormatPlugin )
