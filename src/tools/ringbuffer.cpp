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

#include "ringbuffer.h"




namespace Grim {




static const int ReserveCount = 65536;




class RingBufferPrivate
{
public:
	RingBufferPrivate( RingBuffer * ringBuffer );

	bool seek( qint64 pos );

	qint64 readData( char * data, qint64 maxLen );
	qint64 writeData( const char * data, qint64 maxLen );

	void appendBuffer( qint64 size );

	void push( const char * data, int size );
	void push( const QByteArray & data );
	void pop( int size );
	void clear();

private:
	RingBuffer * ringBuffer_;

	QList<QByteArray> ring_;
	qint64 pos_;
	qint64 size_;
	int ringBufferNumber_;
	qint64 ringBufferPos_;
	qint64 offset_;

	friend class RingBuffer;
};




RingBufferPrivate::RingBufferPrivate( RingBuffer * ringBuffer ) :
	ringBuffer_( ringBuffer ),
	pos_( 0 ),
	size_( 0 ),
	ringBufferNumber_( -1 ),
	offset_( 0 )
{
}


bool RingBufferPrivate::seek( qint64 pos )
{
	Q_ASSERT( pos >= 0 && pos <= size_ );

	pos_ = pos;

	if ( size_ == 0 )
	{
		Q_ASSERT( pos_ == 0 );
		ringBufferNumber_ = -1;
	}
	else
	{
		ringBufferPos_ = pos_ + offset_;
		for ( ringBufferNumber_ = 0; ; ++ringBufferNumber_ )
		{
			const qint64 ringBufferSize = ring_.at( ringBufferNumber_ ).size();

			if ( ringBufferPos_ < ringBufferSize )
				break;

			ringBufferPos_ -= ringBufferSize;

			if ( ringBufferPos_ == 0 )
			{
				// seek right to end of ring buffer
				Q_ASSERT( pos_ == size_ );
				ringBufferNumber_ = -1;
				break;
			}
		}
	}

	return true;
}


qint64 RingBufferPrivate::readData( char * data, qint64 maxLen )
{
	Q_ASSERT( maxLen >= 0 );

	if ( ringBufferNumber_ == -1 || maxLen == 0 || pos_ == size_ )
		return 0;

	qint64 bytesRead = 0;

	char * p = data;
	qint64 remains = maxLen;
	while ( ringBufferNumber_ != -1 && remains != 0 )
	{
		const QByteArray & buffer = ring_.at( ringBufferNumber_ );
		const qint64 bufferSize = buffer.size();

		Q_ASSERT( ringBufferPos_ < bufferSize );

		const qint64 bytesToCopy = qMin<int>( remains, bufferSize - ringBufferPos_ );
		memcpy( p, buffer.constData() + ringBufferPos_, bytesToCopy );

		p += bytesToCopy;
		bytesRead += bytesToCopy;
		remains -= bytesToCopy;

		// seek forward
		pos_ += bytesToCopy;
		ringBufferPos_ += bytesToCopy;
		if ( ringBufferPos_ == bufferSize )
		{
			ringBufferNumber_++;
			ringBufferPos_ = 0;

			if ( ringBufferNumber_ == ring_.count() )
			{
				ringBufferNumber_ = -1;
				// assert that we at end
				Q_ASSERT( pos_ == size_ );
			}
		}
	}

	return bytesRead;
}


qint64 RingBufferPrivate::writeData( const char * data, qint64 maxLen )
{
	push( data, maxLen );
	return maxLen;
}


void RingBufferPrivate::appendBuffer( qint64 size )
{
	QByteArray buffer;
	buffer.reserve( qMax<qint64>( size, ReserveCount ) );
	ring_ << buffer;
}


void RingBufferPrivate::push( const char * data, int size )
{
	Q_ASSERT( size >= 0 );

	if ( size == 0 )
		return;

	const char * p = data;
	int remains = size;
	while ( remains > 0 )
	{
		if ( ring_.isEmpty() )
		{
			appendBuffer( remains );
		}
		else
		{
			const QByteArray & buffer = ring_.last();
			if ( buffer.size() == buffer.capacity() )
				appendBuffer( remains );
		}

		QByteArray & buffer = ring_.last();

		const int bytesToCopy = qMin<int>( buffer.capacity() - buffer.size(), remains );

		const int writePos = buffer.size();
		buffer.resize( writePos + bytesToCopy );
		memcpy( buffer.data() + writePos, p, bytesToCopy );

		p += bytesToCopy;
		remains -= bytesToCopy;
	}

	size_ += size;
}


void RingBufferPrivate::push( const QByteArray & data )
{
	if ( data.size() == 0 )
		return;

	ring_ << data;

	size_ += data.size();
}


void RingBufferPrivate::pop( int size )
{
	Q_ASSERT( size <= size_ );

	int remains = size;
	while ( remains > 0 )
	{
		QByteArray & buffer = ring_.first();

		const int bytesToCut = qMin<int>( remains, buffer.size() - offset_ );
		Q_ASSERT( bytesToCut > 0 );

		if ( bytesToCut == buffer.size() - offset_ )
		{
			ring_.removeFirst();
			offset_ = 0;
		}
		else
		{
			offset_ += bytesToCut;
		}

		remains -= bytesToCut;
	}

	size_ -= size;

	seek( 0 );
}


void RingBufferPrivate::clear()
{
	size_ = 0;
	offset_ = 0;
	ring_.clear();

	seek( 0 );
}




/**
 * \class RingBuffer
 *
 * \ingroup tools_module
 *
 * \brief The RingBuffer class represents FIFO buffer with QIODevice interface.
 *
 * Ring buffer is handy, when you have sequential FIFO stream and should collect
 * bytes from it until solid block of useful data to extract will arrive.
 *
 * Such typical stream is a TCP socket, that gives you bytes portion by portion
 * and you can't know when transmitted solid packet will arrive until you parse it fully.
 *
 * In this case you can simply put received bytes into ring buffer and try to extract something from it.
 * If packet still is not solid - wait for more bytes, append it to ring buffer and try again.
 *
 * RingBuffer cares about memory allocation, and never rearranges internal data chunks while reading
 * data by small pieces.
 *
 * Example of typical usage:
 *
 * \code
 * void MyNetworkHandler::readyRead()
 * {
 *     // appending all received bytes
 *     ringBuffer_.push( tcpSocket_->readAll() );
 *
 *     // opening buffer for reading
 *     ringBuffer_.open( QIODevice::ReadOnly );
 *     QDataStream stream( &ringBuffer_ );
 *
 *     // for example we are waiting a solid block of 'int', followed by 'QString'
 *     int myInt;
 *     QString myString;
 *     stream >> myInt;
 *     stream >> myString;
 *
 *     // check if our block is solid
 *     if ( stream.status() == QDataStream::ReadPastEnd )
 *     {
 *         // not enough bytes in the stream, will wait for the next readyRead() call
 *         ringBuffer_.close();
 *         return;
 *     }
 *
 *     // check that our block encountered error, because of received junk from the network
 *     if ( stream.status() == QDataStream::ReadCorruptedData )
 *     {
 *         ringBuffer_.close();
 *         tcpSocket_->close();
 *         return;
 *     }
 *
 *     // here myInt and myString variables are valid, we can process them
 *     qDebug() << "Received from network:" << myInt << myString;
 *
 *     // and remove handled bytes from ring buffer
 *     const qint64 wellReadPos = ringBuffer.pos();
 *     ringBuffer_.close();
 *     ringBuffer_.pop( wellReadPos );
 * }
 * \endcode
 */


/**
 * Constructs empty ring buffer.
 * \a parent is passed to the QObject constructor.
 */

RingBuffer::RingBuffer( QObject * parent ) :
	d_( new RingBufferPrivate( this ) )
{
}


/**
 * Destroys the ring buffer.
 */

RingBuffer::~RingBuffer()
{
	delete d_;
}


/**
 * Appends the given \a data bytes of the \a size to the buffer.
 * Note that buffer must be closed.
 *
 * \sa pop()
 */

void RingBuffer::push( const char * data, int size )
{
	Q_ASSERT( !isOpen() );

	d_->push( data, size );
}


/**
 * \overload
 *
 * Appends the given \a data byte array to the buffer.
 * Note that buffer must be closed.
 *
 * \sa pop()
 */

void RingBuffer::push( const QByteArray & data )
{
	Q_ASSERT( !isOpen() );

	d_->push( data );
}


/**
 * Removes \a size bytes from the start of the buffer.
 * Note that buffer must be closed.
 *
 * \sa push()
 */

void RingBuffer::pop( int size )
{
	Q_ASSERT( !isOpen() );

	d_->pop( size );
}


/**
 * Clears the buffer. Equivalent of pop( size() ).
 * Note that buffer must be closed.
 */

void RingBuffer::clear()
{
	Q_ASSERT( !isOpen() );

	d_->clear();
}


/** \internal
 */

bool RingBuffer::open( QIODevice::OpenMode openMode )
{
	if ( openMode == QIODevice::ReadOnly )
	{
		d_->seek( 0 );
	}
	else if ( openMode & QIODevice::WriteOnly && openMode & QIODevice::Append )
	{
	}
	else
	{
		Q_ASSERT_X( false,
			"Grim::RingBuffer::open()",
			"Only (ReadOnly) and (WriteOnly|Append) modes are allowed." );
	}

	return QIODevice::open( openMode );
}


/** \internal
 */

bool RingBuffer::isSequential() const
{
	return false;
}


/** \internal
 */

qint64 RingBuffer::bytesAvailable() const
{
	return d_->size_ - d_->pos_ + QIODevice::bytesAvailable();
}


/** \internal
 */

qint64 RingBuffer::size() const
{
	return d_->size_;
}


/** \internal
 */

bool RingBuffer::seek( qint64 pos )
{
	QIODevice::seek( pos );
	return d_->seek( pos );
}


/** \internal
 */

qint64 RingBuffer::readData( char * data, qint64 maxLen )
{
	return d_->readData( data, maxLen );
}


/** \internal
 */

qint64 RingBuffer::writeData( const char * data, qint64 maxLen )
{
	return d_->writeData( data, maxLen );
}




} // namespace Grim
