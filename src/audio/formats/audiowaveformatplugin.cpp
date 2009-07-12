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

#include "audiowaveformatplugin.h"

#include <QDataStream>
#include <QStringList>




namespace Grim {




static const QByteArray RawFormatName = "Raw";
static const QByteArray WaveFormatName = "Wave";
static const QByteArray AuFormatName = "AU";


static const quint32 RiffMagic = 0x52494646; // first 4 bytes of .wav file: "RIFF"
static const quint32 WaveMagic = 0x57415645; // 4 bytes of .wav file: "WAVE"
static const quint32 FmtMagic  = 0x666d7420; // 4 bytes of .wav file: "fmt"
static const quint32 DataMagic = 0x64617461; // 4 bytes of .wav file: "data"

static const quint32 AuMagic   = 0x2E736E64; // first 4 bytes of .au file:  ".snd"
static const int AuHeaderSize = 24;

enum AuEncoding
{
	AuEncoding_ULaw_8   = 1,  // 8-bit ISDN u-law
	AuEncoding_Pcm_8    = 2,  // 8-bit linear PCM (signed)
	AuEncoding_Pcm_16   = 3,  // 16-bit linear PCM (signed, big-endian)
	AuEncoding_Pcm_24   = 4,  // 24-bit linear PCM
	AuEncoding_Pcm_32   = 5,  // 32-bit linear PCM
	AuEncoding_Float_32 = 6,  // 32-bit IEEE floating point
	AuEncoding_Float_64 = 7,  // 64-bit IEEE floating point
	AuEncoding_ALaw_8   = 27  // 8-bit ISDN a-law
};




static void _linearCodec( const void * data, int size, void * outData )
{
	memcpy( outData, data, size );
}


static void _pcm8sCodec( const void * data, int size, void * outData )
{
	const qint8 * d = (const qint8*)data;
	qint8 * outd = (qint8*)outData;

	for ( int i = 0; i < size; i++ )
		outd[ i ] = d[ i ] + qint8( 128 );
}


static void _pcm16Codec( const void * data, int size, void * outData )
{
	const qint16 * d = (const qint16*)data;
	const int isize = size / 2;
	qint16 * outd = (qint16*)outData;

	for ( int i = 0; i < isize; i++ )
    {
		qint16 x = d[ i ];
		outd[ i ] = ((x << 8) & 0xFF00) | ((x >> 8) & 0x00FF);
    }
}


inline static qint16 _mulaw2linear( quint8 mulawbyte )
{
	static const qint16 exp_lut[8] = {
		0, 132, 396, 924, 1980, 4092, 8316, 16764
	};

	qint16 sign, exponent, mantissa, sample;
	mulawbyte = ~mulawbyte;
	sign = (mulawbyte & 0x80);
	exponent = (mulawbyte >> 4) & 0x07;
	mantissa = mulawbyte & 0x0F;
	sample = exp_lut[exponent] + (mantissa << (exponent + 3));
	if ( sign != 0 )
		sample = -sample;
	return sample;
}


static void _uLawCodec(  const void * data, int size, void * outData  )
{
	const qint8 * d = (const qint8*)data;
	qint16 * outd = (qint16*)outData;

	for ( int i = 0; i < size; i++ )
		outd[ i ] = _mulaw2linear( d[ i ] );
}


inline static qint16 _alaw2linear( quint8 a_val )
{
	static const quint8 SignBit   = 0x80; // Sign bit for a A-law byte.
	static const quint8 QuantMask = 0x0f; // Quantization field mask.
	static const int SegShift     = 4;    // Left shift for segment number.
	static const quint8 SegMask   = 0x70; // Segment field mask.

	qint16 t, seg;
	a_val ^= 0x55;
	t = (a_val & QuantMask) << 4;
	seg = ((qint16) a_val & SegMask) >> SegShift;
	switch ( seg )
	{
	case 0:
		t += 8;
		break;
	case 1:
		t += 0x108;
		break;
	default:
		t += 0x108;
		t <<= seg - 1;
	}
	return (a_val & SignBit) ? t : -t;
}


static void _aLawCodec(  const void * data, int size, void * outData  )
{
	const qint8 * d = (const qint8*)data;
	qint16 * outd = (qint16*)outData;

	for ( int i = 0; i < size; i++ )
		outd[ i ] = _alaw2linear( d[ i ] );
}




AudioWaveFormatDevice::AudioWaveFormatDevice( AudioWaveFormatFile * formatFile ) :
	formatFile_( formatFile )
{
	isWave_ = false;
	isAu_ = false;

	pos_ = -1;
}


AudioWaveFormatDevice::~AudioWaveFormatDevice()
{
	close();
}


inline int AudioWaveFormatDevice::_codecMultiplier() const
{
	Q_ASSERT( codec_ );
	return codec_ == _linearCodec || codec_ == _pcm8sCodec || codec_ == _pcm16Codec ? 1 : 2;
}


bool AudioWaveFormatDevice::_guessWave()
{
	QDataStream ds( &file_ );
	ds.setByteOrder( QDataStream::BigEndian );

	quint32 magic;
	ds >> magic;

	if ( ds.status() != QDataStream::Ok )
		return false;

	if ( magic != RiffMagic )
		return false;

	isWave_ = true;
	formatFile_->setResolvedFormat( WaveFormatName );

	return true;
}


bool AudioWaveFormatDevice::_guessAu()
{
	QDataStream ds( &file_ );
	ds.setByteOrder( QDataStream::BigEndian );

	quint32 magic;
	ds >> magic;

	if ( ds.status() != QDataStream::Ok )
		return false;

	if ( magic != AuMagic )
		return false;

	isAu_ = true;
	formatFile_->setResolvedFormat( AuFormatName );

	return true;
}


void AudioWaveFormatDevice::_setOutputSize()
{
	outputSize_ = formatFile_->truncatedSize( inputSize_ * _codecMultiplier() );
	formatFile_->setTotalSamples( outputSize_ / (formatFile_->channels() * (formatFile_->bitsPerSample() >> 3)) );
}


bool AudioWaveFormatDevice::_openRaw()
{
	// hardcoded properties for raw data
	formatFile_->setResolvedFormat( RawFormatName );
	formatFile_->setChannels( 1 );
	formatFile_->setBitsPerSample( 8 );
	formatFile_->setFrequency( 8000 );

	codec_ = _linearCodec;
	inputSize_ = file_.size();

	_setOutputSize();

	dataPos_ = file_.pos();

	return true;
}


bool AudioWaveFormatDevice::_openWave()
{
	QDataStream ds( &file_ );

	quint32 chunkLength;
	ds.setByteOrder( QDataStream::LittleEndian );
	ds >> chunkLength;

	{
		ds.setByteOrder( QDataStream::BigEndian );
		quint32 magic;
		ds >> magic;
		if ( magic != WaveMagic )
			return false;
	}

	if ( ds.status() != QDataStream::Ok )
		return false;

	quint16 audioFormat;
	quint16 channels;
	quint32 frequency;
	quint32 byteRate;
	quint16 blockAlign;
	quint16 bitsPerSample;

	bool foundHeader = false;

	while ( true )
	{
		quint32 magic;
		quint32 chunkLength;

		ds.setByteOrder( QDataStream::BigEndian );
		ds >> magic;

		ds.setByteOrder( QDataStream::LittleEndian );
		ds >> chunkLength;

		if ( ds.status() != QDataStream::Ok )
			return false;

		if ( magic == FmtMagic )
		{
			foundHeader = true;

			if ( chunkLength < 16 )
				return false;

			ds.setByteOrder( QDataStream::LittleEndian );
			ds >> audioFormat;
			ds >> channels;
			ds >> frequency;
			ds >> byteRate;
			ds >> blockAlign;
			ds >> bitsPerSample;

			if ( ds.status() != QDataStream::Ok )
				return false;

			if ( !file_.seek( file_.pos() + chunkLength - 16 ) )
				return false;

			switch ( audioFormat )
			{
			case 1: // PCM
				codec_ =
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
					_linearCodec;
#else
					bitsPerSample == 8 ? _linearCodec : _pcm16Codec;
#endif
				break;

			case 7: // uLaw
				bitsPerSample *= 2;
				codec_ = _uLawCodec;
				break;

			default:
				return false;
			}
		}
		else if ( magic == DataMagic )
		{
			if ( !foundHeader )
				return false;

			Q_ASSERT( codec_ );
			inputSize_ = chunkLength;

			formatFile_->setResolvedFormat( WaveFormatName );
			formatFile_->setChannels( channels );
			formatFile_->setFrequency( frequency );
			formatFile_->setBitsPerSample( bitsPerSample );

			_setOutputSize();

			dataPos_ = file_.pos();

			return true;
		}
		else
		{
			if ( !file_.seek( file_.pos() + chunkLength ) )
				return false;
		}

		if ( (chunkLength & 1) && !file_.atEnd() )
		{
			if ( !file_.seek( file_.pos() + 1 ) )
				return false;
		}
	}

	return false;
}


bool AudioWaveFormatDevice::_openAu()
{
	QDataStream ds( &file_ );

	qint32 dataOffset;    // byte offset to data part, minimum 24
	qint32 dataLength;    // number of bytes in the data part, -1 = not known
	qint32 dataEncoding;  // encoding of the data part, see AUEncoding
	qint32 frequency;     // number of samples per second
	qint32 channels;      // number of interleaved channels

	int bitsPerSample = 0;

	ds.setByteOrder( QDataStream::BigEndian );
	ds >> dataOffset;
	ds >> dataLength;
	ds >> dataEncoding;
	ds >> frequency;
	ds >> channels;

	if ( ds.status() != QDataStream::Ok )
		return false;

	if ( dataLength == -1 )
		dataLength = file_.bytesAvailable() - AuHeaderSize - dataOffset;

	if ( dataOffset < AuHeaderSize || dataLength <= 0 || frequency < 1 || channels < 1 )
		return false;

	if ( !file_.seek( file_.pos() + dataOffset - AuHeaderSize ) )
		return false;

	switch ( dataEncoding )
	{
	case AuEncoding_ULaw_8:
		bitsPerSample = 16;
		codec_ = _uLawCodec;
		break;

	case AuEncoding_Pcm_8:
		bitsPerSample = 8;
		codec_ = _pcm8sCodec;
		break;

	case AuEncoding_Pcm_16:
		bitsPerSample = 16;
		codec_ =
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
			_pcm16Codec;
#else
			_linearCodec;
#endif
		break;

	case AuEncoding_ALaw_8:
		bitsPerSample = 16;
		codec_ = _aLawCodec;
		break;

	default:
		return false;
	}

	inputSize_ = dataLength;

	formatFile_->setResolvedFormat( AuFormatName );
	formatFile_->setChannels( channels );
	formatFile_->setFrequency( frequency );
	formatFile_->setBitsPerSample( bitsPerSample );

	_setOutputSize();

	dataPos_ = file_.pos();

	return true;
}


qint64 AudioWaveFormatDevice::_readData( char * data, qint64 maxSize )
{
	qint64 bytesToRead = qMin( outputSize_ - pos_, maxSize );

	QByteArray buffer;
	buffer.resize( bytesToRead / _codecMultiplier() );
	if ( file_.read( buffer.data(), buffer.size() ) != buffer.size() )
		return -1;

	codec_( buffer.constData(), buffer.size(), data );

	pos_ += bytesToRead;

	return bytesToRead;
}


bool AudioWaveFormatDevice::_open( const QByteArray & format, bool firstTime )
{
	if ( !firstTime )
		Q_ASSERT( !format.isEmpty() );

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

	isWave_ = false;
	isAu_ = false;

	// resolve actual format
	if ( format.isNull() )
	{
		bool isWaveChecked = false;
		bool isAuChecked = false;

		// first check formats by extension
		if ( file_.fileName().endsWith( QLatin1String( ".wav" ), Qt::CaseInsensitive ) )
		{
			isWaveChecked = true;
			if ( _guessWave() )
				goto formatIsChecked;
		}

		if ( file_.fileName().endsWith( QLatin1String( ".au" ), Qt::CaseInsensitive ) )
		{
			isAuChecked = true;
			if ( _guessAu() )
				goto formatIsChecked;
		}

		// not extensions or extension not accorded to file contents
		// ignore extension and check by contents only
		if ( !isWaveChecked && _guessWave() )
			goto formatIsChecked;

		if ( !isAuChecked && _guessAu() )
			goto formatIsChecked;

		// unknown file format, we will not treat it as raw
		file_.close();
		return false;
	}

	formatIsChecked:

	const QByteArray actualFormat = !formatFile_->format().isNull() ? formatFile_->format() : formatFile_->resolvedFormat();
	Q_ASSERT( !actualFormat.isNull() );

	bool isOpened = false;
	if ( actualFormat == RawFormatName )
	{
		isOpened = _openRaw();
	}
	else if ( actualFormat == WaveFormatName )
	{
		isWave_ = true;
		if ( !format.isNull() && !_guessWave() )
		{
			file_.close();
			return false;
		}

		isOpened = _openWave();
	}
	else if ( actualFormat == AuFormatName )
	{
		isAu_ = true;
		if ( !format.isNull() && !_guessAu() )
		{
			file_.close();
			return false;
		}
		isOpened = _openAu();
	}
	else
	{
		Q_ASSERT( false );
	}

	if ( !isOpened )
	{
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
			file_.close();
			return false;
		}
	}

	pos_ = 0;

	return true;
}


bool AudioWaveFormatDevice::open( QIODevice::OpenMode openMode )
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


void AudioWaveFormatDevice::_close()
{
	if ( pos_ == -1 )
		return;

	pos_ = -1;
}


void AudioWaveFormatDevice::close()
{
	if ( !isOpen() )
		return;

	QIODevice::close();

	_close();

	file_.close();
}


bool AudioWaveFormatDevice::isSequential() const
{
	Q_ASSERT( isOpen() );
	return isSequential_;
}


qint64 AudioWaveFormatDevice::pos() const
{
	return pos_;
}


bool AudioWaveFormatDevice::atEnd() const
{
	Q_ASSERT( isOpen() );

	return pos_ == outputSize_;
}


qint64 AudioWaveFormatDevice::size() const
{
	if ( !isOpen() )
		return 0;

	return outputSize_;
}


qint64 AudioWaveFormatDevice::bytesAvailable() const
{
	if ( isSequential_ )
		return outputSize_ - pos_ + QIODevice::bytesAvailable();
	else
		return outputSize_ - pos_;
}


qint64 AudioWaveFormatDevice::readData( char * data, qint64 maxSize )
{
	Q_ASSERT( isOpen() );

	return _readData( data, formatFile_->truncatedSize( maxSize ) );
}


qint64 AudioWaveFormatDevice::writeData( const char * data, qint64 maxSize )
{
	return -1;
}


bool AudioWaveFormatDevice::seek( qint64 pos )
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

	// Same as for Vorbis and FLAC.
	Q_ASSERT( !isSequential_ );

	// this just asserts correct pos value
	formatFile_->bytesToSamples( pos );

	const qint64 outputPos = dataPos_ + pos / _codecMultiplier();

	if ( !file_.seek( outputPos ) )
		return false;

	pos_ = pos;

	return true;
}




AudioWaveFormatFile::AudioWaveFormatFile( const QString & fileName, const QByteArray & format ) :
	AudioFormatFile( fileName, format ), device_( this )
{
}


QIODevice * AudioWaveFormatFile::device()
{
	return &device_;
}




QList<QByteArray> AudioWaveFormatPlugin::formats() const
{
	return QList<QByteArray>()
		<< RawFormatName
		<< WaveFormatName
		<< AuFormatName;
}


QStringList AudioWaveFormatPlugin::extensions() const
{
	return QStringList()
		<< QLatin1String( "raw" )
		<< QLatin1String( "wav" )
		<< QLatin1String( "au" );
}


AudioFormatFile * AudioWaveFormatPlugin::createFile( const QString & fileName, const QByteArray & format )
{
	if ( !format.isNull() )
		Q_ASSERT( formats().contains( format ) );

	return new AudioWaveFormatFile( fileName, format );
}




} // namespace Grim




Q_EXPORT_PLUGIN2( grim_audio_wave_format_plugin, Grim::AudioWaveFormatPlugin )
