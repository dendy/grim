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

#include "audiobufferloader.h"

#include <QDebug>

#include "audio_p.h"
#include "audioformatplugin.h"




namespace Grim {




/** \internal
 * \class AudioBufferLoader
 */




AudioBufferLoader::AudioBufferLoader( QObject * parent ) :
	QThread( parent )
{
	isAborted_ = false;
}


AudioBufferLoader::~AudioBufferLoader()
{
	abort();
}


void AudioBufferLoader::abort()
{
	{
		QMutexLocker moreRequestsLocker( &moreRequestsMutex_ );
		isAborted_ = true;
		moreRequestsWaiter_.wakeOne();
	}

	wait();

	QWriteLocker requestsLocker( &requestsMutex_ );

	requestForId_.clear();
	requests_.clear();
	prioritizedRequests_.clear();
	idGenerator_.reset();
}


void AudioBufferLoader::_addRequest( const Request & request )
{
	QWriteLocker requestsLocker( &requestsMutex_ );

	if ( request.isPrioritized )
		prioritizedRequests_ << request.requestId;
	else
		requests_ << request.requestId;

	requestForId_[ request.requestId ] = request;

	if ( !isRunning() )
	{
		isAborted_ = false;
		isWaitingForMoreRequests_ = false;
		start();
	}
	else
	{
		QMutexLocker moreRequestsLocker( &moreRequestsMutex_ );
		if ( isWaitingForMoreRequests_ )
			moreRequestsWaiter_.wakeOne();
	}
}


void AudioBufferLoader::_removeRequest( int requestId )
{
	Request request = requestForId_.take( requestId );
	if ( request.isPrioritized )
		prioritizedRequests_.removeOne( requestId );
	else
		requests_.removeOne( requestId );

	idGenerator_.free( requestId );
}


int AudioBufferLoader::addRequest( const QString & fileName, const QByteArray & format,
	qint64 sampleOffset, qint64 sampleCount, bool closeFile, bool isPrioritized )
{
	Request request;
	request.isRunning = false;
	request.requestId = idGenerator_.take();
	request.isCancelled = false;
	request.isPrioritized = isPrioritized;
	request.fileName = fileName;
	request.format = format;
	request.file = 0;
	request.sampleOffset = sampleOffset;
	request.sampleCount = sampleCount;
	request.closeFile = closeFile;

	_addRequest( request );

	return request.requestId;
}


int AudioBufferLoader::addRequest( AudioFormatFile * file,
	qint64 sampleOffset, qint64 sampleCount, bool closeFile, bool isPrioritized )
{
	Request request;
	request.isRunning = false;
	request.requestId = idGenerator_.take();
	request.isCancelled = false;
	request.isPrioritized = isPrioritized;
	request.file = file;
	request.sampleOffset = sampleOffset;
	request.sampleCount = sampleCount;
	request.closeFile = closeFile;

	_addRequest( request );

	return request.requestId;
}


void AudioBufferLoader::cancelRequest( int requestId )
{
	QWriteLocker requestsLocker( &requestsMutex_ );

	Q_ASSERT( requestForId_.contains( requestId ) );

	Request & request = requestForId_[ requestId ];

	if ( request.isRunning )
	{
		request.isCancelled = true;
		return;
	}

	_removeRequest( requestId );
}


void AudioBufferLoader::increasePriority( int requestId )
{
	QWriteLocker requestsLocker( &requestsMutex_ );

	Q_ASSERT( requestForId_.contains( requestId ) );

	Request & request = requestForId_[ requestId ];

	if ( request.isPrioritized || request.isRunning )
		return;

	requests_.removeOne( requestId );
	prioritizedRequests_ << requestId;
}


void AudioBufferLoader::_threadRequestFinished( int requestId, void * resultp )
{
	QWriteLocker requestsLocker( &requestsMutex_ );

	Result * result = static_cast<Result*>( resultp );

	if ( isAborted_ )
	{
		if ( result->file )
			delete result->file;
		return;
	}

	Q_ASSERT( requestForId_.contains( requestId ) );

	Request request = requestForId_.value( requestId );

	if ( !request.isCancelled )
	{
		emit requestFinished( requestId, resultp );
	}
	else
	{
		if ( result->file )
			delete result->file;
	}

	_removeRequest( requestId );
}


void AudioBufferLoader::run()
{
	while ( true )
	{
		if ( isAborted_ )
			break;

		Request * request = 0;
		{
			QWriteLocker requestsLocker( &requestsMutex_ );

			// collect first suitable request
			int requestId = 0;
			if ( !prioritizedRequests_.isEmpty() )
				requestId = prioritizedRequests_.first();
			else if ( !requests_.isEmpty() )
				requestId = requests_.first();

			if ( requestId != 0 )
			{
				request = &requestForId_[ requestId ];
				request->isRunning = true;
			}
		}

		if ( !request )
		{
			QMutexLocker moreRequestsLocker( &moreRequestsMutex_ );
			isWaitingForMoreRequests_ = true;
			moreRequestsWaiter_.wait( &moreRequestsMutex_ );
			isWaitingForMoreRequests_ = false;
			continue;
		}

		Result result;
		const bool done = _processRequest( request, &result );
		result.hasError = !done;

		{
			bool invoked = QMetaObject::invokeMethod( this, "_threadRequestFinished", Qt::BlockingQueuedConnection,
				Q_ARG(int,request->requestId), Q_ARG(void*,&result) );
			Q_UNUSED( invoked );
			Q_ASSERT( invoked );
		}
	}
}


bool AudioBufferLoader::_processRequest( Request * request, Result * result )
{
	result->file = 0;

	if ( request->file )
	{
		// file can be closed and opened spontaneously
		// always check this and open if needed
		if ( !request->file->device()->isOpen() )
		{
			if ( !request->file->device()->open( QIODevice::ReadOnly ) )
			{
				delete request->file;
				request->file = 0;
				return false;
			}
		}

		result->file = request->file;
	}
	else
	{
		result->file = AudioManagerPrivate::sharedManagerPrivate()->createFormatFile( request->fileName, request->format );
		if ( !result->file )
			return false;
	}

	// at this stage file must be opened
	Q_ASSERT( result->file->device()->isOpen() );

	result->content.isSequential = result->file->device()->isSequential();
	result->content.channels = result->file->channels();
	result->content.bitsPerSample = result->file->bitsPerSample();
	result->content.frequency = result->file->frequency();
	result->content.totalSamples = result->file->totalSamples();
	result->content.samplesOffset = 0;
	result->content.samples = 0;

	if ( !AudioManagerPrivate::sharedManagerPrivate()->verifyOpenALBuffer( result->content ) )
		return false;

	// seek to desired sample offset
	const qint64 bytesOffset = request->sampleOffset == -1 ? -1 : result->file->samplesToBytes( request->sampleOffset );

	if ( request->sampleOffset != -1 && bytesOffset != result->file->device()->pos() )
	{
		if ( result->file->device()->isSequential() )
		{
			bool isSeeked = false;

			// if offset is 0 try to seek
			if ( bytesOffset == 0 )
				isSeeked = result->file->device()->seek( 0 );

			if ( !isSeeked )
			{
				// Two policies here.
#if 0
				// Policy 1: Closing/Opening.

				// if seek pos is below current pos then close file and reopen file
				if ( bytesOffset < result->file->device()->pos() )
				{
					result->file->device()->close();
					if ( !result->file->device()->open( QIODevice::ReadOnly ) )
						return false;
				}

				// then try to seek ahead to seek pos if device allows this
				if ( !result->file->device()->seek( bytesOffset ) )
					return false;
#endif
#if 1
				// Policy 2: Deny seeking.
				return false;
#endif
			}
		}
		else
		{
			// device is random access
			if ( !result->file->device()->seek( bytesOffset ) )
				return false;
		}
	}

	const qint64 currentBytesOffset = result->file->device()->pos();
	const qint64 currentSamplesOffset = result->file->bytesToSamples( currentBytesOffset );
	const qint64 totalSamples = result->file->totalSamples();

	// calculate count of samples actually to read
	bool isCountOfSamplesToReadExact;
	qint64 samplesToRead;
	if ( totalSamples == -1 )
	{
		if ( request->sampleCount == -1 )
		{
			// read whole unknown sized device thru the end
			samplesToRead = -1;
		}
		else
		{
			// read requested count of samples or less
			samplesToRead = request->sampleCount;
			isCountOfSamplesToReadExact = false;
		}
	}
	else
	{
		if ( request->sampleCount == -1 )
		{
			// read rest samples
			samplesToRead = totalSamples - currentSamplesOffset;
		}
		else
		{
			// read minimum of rest and requested samples
			samplesToRead = qMin( request->sampleCount, totalSamples - currentSamplesOffset );
		}
		isCountOfSamplesToReadExact = true;
	}

	if ( samplesToRead == -1 )
	{
		static const int BlockSize = 65536;

		qint64 totalBufferSize = 0;
		QList<QByteArray> buffers;
		while ( true )
		{
			QByteArray buffer;
			buffer.resize( BlockSize );

			const qint64 bytesReaded = result->file->device()->read( buffer.data(), BlockSize );
			if ( bytesReaded == -1 )
			{
#ifdef GRIM_AUDIO_DEBUG
				qDebug() << "AudioBufferLoader::_processRequest() : Error reading unknown count of samples.";
#endif
				return false;
			}

			if ( bytesReaded == 0 )
				break;

			totalBufferSize += bytesReaded;

			buffer.truncate( bytesReaded );
			buffers << buffer;

			if ( bytesReaded < BlockSize )
				break;
		}

		if ( totalBufferSize == 0 )
		{
			result->content.samples = 0;
		}
		else
		{
			QByteArray totalBuffer;
			totalBuffer.resize( totalBufferSize );

			qint64 offset = 0;
			for ( QListIterator<QByteArray> it( buffers ); it.hasNext(); )
			{
				const QByteArray & buffer = it.next();
				memcpy( totalBuffer.data() + offset, buffer.constData(), buffer.size() );
				offset += buffer.size();
			}

			result->content.data = totalBuffer;
			result->content.samples = result->file->bytesToSamples( totalBufferSize );
		}
	}
	else // samplesToRead != -1
	{
		if ( samplesToRead == 0 )
		{
			// nothing to read
			result->content.samples = 0;
		}
		else
		{
			// read stream until bytesToRead reached
			const qint64 bytesToRead = result->file->samplesToBytes( samplesToRead );

			QByteArray totalBuffer;
			totalBuffer.resize( bytesToRead );

			const qint64 bytesReaded = result->file->device()->read( totalBuffer.data(), bytesToRead );
			if ( bytesReaded == -1 )
			{
#ifdef GRIM_AUDIO_DEBUG
				qDebug() << "AudioBufferLoader::_processRequest() : Error reading exact count of samples.";
#endif
				return false;
			}

			if ( isCountOfSamplesToReadExact && bytesReaded != bytesToRead )
			{
#ifdef GRIM_AUDIO_DEBUG
				qDebug() << "AudioBufferLoader::_processRequest() : Count of samples read and requested not matched.";
#endif
				return false;
			}

			totalBuffer.truncate( bytesReaded );

			result->content.data = totalBuffer;
			result->content.samples = result->file->bytesToSamples( bytesReaded );
		}
	}

	result->content.samplesOffset = currentSamplesOffset;

	if ( request->closeFile )
	{
		delete result->file;
		result->file = 0;
	}

	return true;
}




} // namespace Grim
