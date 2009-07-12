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

#include "audiovorbisformatplugin.h"

#include <QStringList>
#include <QDebug>




namespace Grim {




static const QByteArray VorbisFormatName = "Ogg/Vorbis";

static const int VorbisEndianess =
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	0;
#else
	1;
#endif

static const int VorbisBytesPerSample = 2;




inline static AudioVorbisFormatDevice * _takeVorbisDevice( void * data )
{
	return static_cast<AudioVorbisFormatDevice*>( data );
}


size_t AudioVorbisFormatDevice::ogg_read( void * ptr, size_t size, size_t nmemb, void * datasource )
{
	AudioVorbisFormatDevice * device = _takeVorbisDevice( datasource );

	const qint64 bytesReaded = device->file_.read( (char*)ptr, nmemb );
	Q_ASSERT( bytesReaded >= -1 );

	if ( bytesReaded == -1 )
	{
		// vorbisfile has no ability to detect aborted stream.
		// We have to save error flag somewhere else and tell vorbisfile
		// that stream just finished.
		device->readError_ = true;
		return 0;
	}

	return (size_t)bytesReaded;
}


int AudioVorbisFormatDevice::ogg_seek( void * datasource, ogg_int64_t offset, int whence )
{
	AudioVorbisFormatDevice * device = _takeVorbisDevice( datasource );

	qint64 absPos = -1;
	switch ( whence )
	{
	case SEEK_SET:
		absPos = offset;
		break;
	case SEEK_CUR:
		absPos = device->file_.pos() + offset;
		break;
	case SEEK_END:
		absPos = device->file_.size() - offset;
		break;
	default:
		return -1;
	}

	const bool done = device->file_.seek( absPos );

	return done ? 0 : -1;
}


int AudioVorbisFormatDevice::ogg_close( void * datasource )
{
	AudioVorbisFormatDevice * device = _takeVorbisDevice( datasource );

	device->file_.close();

	return 0;
}


long AudioVorbisFormatDevice::ogg_tell( void * datasource )
{
	AudioVorbisFormatDevice * device = _takeVorbisDevice( datasource );

	return device->file_.pos();
}


static ov_callbacks ogg_create_callbacks()
{
	ov_callbacks c;
	c.read_func = AudioVorbisFormatDevice::ogg_read;
	c.seek_func = AudioVorbisFormatDevice::ogg_seek;
	c.close_func = AudioVorbisFormatDevice::ogg_close;
	c.tell_func = AudioVorbisFormatDevice::ogg_tell;
	return c;
}




AudioVorbisFormatDevice::AudioVorbisFormatDevice( AudioVorbisFormatFile * formatFile ) :
	formatFile_( formatFile )
{
	pos_ = -1;
}


AudioVorbisFormatDevice::~AudioVorbisFormatDevice()
{
	close();
}


bool AudioVorbisFormatDevice::_open( const QByteArray & format, bool firstTime )
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

	ov_callbacks ovCallbacks = ogg_create_callbacks();

	if ( isSequential_ )
	{
		ovCallbacks.seek_func = 0;
		ovCallbacks.close_func = 0;       // we will close file manually
		ovCallbacks.tell_func = 0;
	}
	else
	{
		ovCallbacks.close_func = 0;       // we will close file manually
	}

	if ( ov_open_callbacks( (void*)this, &oggVorbisFile_, 0, 0, ovCallbacks ) != 0 )
	{
		file_.close();
		return false;
	}

	vorbis_info * vorbisInfo = ov_info( &oggVorbisFile_, -1 );

	const int channels = vorbisInfo->channels;
	const int frequency = vorbisInfo->rate;
	const int bitsPerSample = VorbisBytesPerSample * 8;

	if ( channels < 0 || frequency < 0 )
	{
		file_.close();
		return false;
	}

	qint64 totalSamples;

	if ( isSequential_ )
	{
		totalSamples = -1;
	}
	else
	{
		totalSamples = ov_pcm_total( &oggVorbisFile_, -1 );
		if ( totalSamples < 0 )
		{
			ov_clear( &oggVorbisFile_ );
			file_.close();
			return false;
		}
	}

	formatFile_->setResolvedFormat( VorbisFormatName );
	formatFile_->setChannels( channels );
	formatFile_->setFrequency( frequency );
	formatFile_->setBitsPerSample( bitsPerSample );
	formatFile_->setTotalSamples( totalSamples );

	if ( !firstTime )
	{
		if ( formatFile_->channels() != previousChannels ||
			formatFile_->bitsPerSample() != previousBitsPerSample ||
			formatFile_->frequency() != previousFrequency ||
			formatFile_->totalSamples() != previousTotalSamples )
		{
			ov_clear( &oggVorbisFile_ );
			file_.close();
			return false;
		}
	}

	outputSize_ = isSequential_ ? -1 : formatFile_->samplesToBytes( formatFile_->totalSamples() );
	pos_ = 0;
	atEnd_ = false;
	readError_ = false;

	return true;
}


bool AudioVorbisFormatDevice::open( QIODevice::OpenMode openMode )
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


void AudioVorbisFormatDevice::_close()
{
	if ( pos_ == -1 )
		return;

	ov_clear( &oggVorbisFile_ );

	pos_ = -1;
}


void AudioVorbisFormatDevice::close()
{
	if ( !isOpen() )
		return;

	QIODevice::close();

	_close();

	file_.close();
}


bool AudioVorbisFormatDevice::isSequential() const
{
	Q_ASSERT( isOpen() );

	return isSequential_;
}


qint64 AudioVorbisFormatDevice::pos() const
{
	Q_ASSERT( isOpen() );

	return pos_;
}


bool AudioVorbisFormatDevice::atEnd() const
{
	Q_ASSERT( isOpen() );

	return isSequential_ ? atEnd_ : pos_ == outputSize_;
}


qint64 AudioVorbisFormatDevice::size() const
{
	if ( !isOpen() )
		return 0;

	return isSequential_ ? 0 : outputSize_;
}


qint64 AudioVorbisFormatDevice::bytesAvailable() const
{
	if ( isSequential_ )
		return 0 + QIODevice::bytesAvailable();
	else
		return outputSize_ - pos_;
}


qint64 AudioVorbisFormatDevice::readData( char * data, qint64 maxSize )
{
	Q_ASSERT( isOpen() );

	qint64 totalBytes = 0;

	if ( isSequential_ )
	{
		if ( atEnd_ )
			return 0;

		qint64 remainBytes = maxSize;

		while ( remainBytes > 0 )
		{
			int stream;
			const qint64 bytesReaded = ov_read( &oggVorbisFile_, data + totalBytes, remainBytes,
				VorbisEndianess, VorbisBytesPerSample, 1, &stream );

			if ( bytesReaded < 0 )
			{
#ifdef GRIM_AUDIO_DEBUG
				qDebug() << "AudioVorbisFormatDevice::readData() : ov_read() error on sequential, code =" << bytesReaded;
#endif
				return -1;
			}

			Q_ASSERT( bytesReaded <= remainBytes );

			if ( readError_ )
				return -1;

			if ( bytesReaded == 0 )
			{
				atEnd_ = true;
				break;
			}

			totalBytes += bytesReaded;
			remainBytes -= bytesReaded;
		}
	}
	else
	{
		qint64 remainBytes = qMin( formatFile_->truncatedSize( maxSize ), outputSize_ - pos_ );

		while ( remainBytes > 0 )
		{
			int stream;
			const qint64 bytesReaded = ov_read( &oggVorbisFile_, data + totalBytes, remainBytes,
				VorbisEndianess, VorbisBytesPerSample, 1, &stream );

			if ( bytesReaded < 0 )
			{
#ifdef GRIM_AUDIO_DEBUG
				qDebug() << "AudioVorbisFormatDevice::readData() : ov_read() error on random-access, code =" << bytesReaded;
#endif
				return -1;
			}

			Q_ASSERT( bytesReaded <= remainBytes );

			if ( bytesReaded == 0 )
			{
				// end of file reached
				if ( remainBytes > 0 )
					return -1;
			}

			totalBytes += bytesReaded;
			remainBytes -= bytesReaded;
		}
	}

	pos_ += totalBytes;

	return totalBytes;
}


qint64 AudioVorbisFormatDevice::writeData( const char * data, qint64 maxSize )
{
	return -1;
}


bool AudioVorbisFormatDevice::seek( qint64 pos )
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

	// Same as in FLAC plugin.
	// Currently there are no routines to reach here in sequential mode.
	// If somebody wants to seek thru the Ogg/Vorbis stream in sequential mode
	// this would be a complex task with internal buffers and hard decoding
	// sequentially thru the whole stream.
	// The better idea is to operate on non compressed random-access files,
	// like ZIP-archive provides together with compressed ones in the same archive.
	Q_ASSERT( !isSequential_ );

	// this also asserts correct pos value
	const qint64 sample = formatFile_->bytesToSamples( pos );

	const int ret = ov_pcm_seek( &oggVorbisFile_, sample );
	if ( ret != 0 )
		return false;

	pos_ = pos;

	return true;
}


AudioVorbisFormatFile::AudioVorbisFormatFile( const QString & fileName, const QByteArray & format ) :
	AudioFormatFile( fileName, format ), device_( this )
{
}


QIODevice * AudioVorbisFormatFile::device()
{
	return &device_;
}




QList<QByteArray> AudioVorbisFormatPlugin::formats() const
{
	return QList<QByteArray>() << VorbisFormatName;
}


QStringList AudioVorbisFormatPlugin::extensions() const
{
	return QStringList()
		<< QLatin1String( "ogg" )
		<< QLatin1String( "oga" );
}


AudioFormatFile * AudioVorbisFormatPlugin::createFile( const QString & fileName, const QByteArray & format )
{
	if ( !format.isNull() )
		Q_ASSERT( formats().contains( format ) );

	return new AudioVorbisFormatFile( fileName, format );
}




} // namespace Grim




Q_EXPORT_PLUGIN2( grim_audio_vorbis_format_plugin, Grim::AudioVorbisFormatPlugin )
