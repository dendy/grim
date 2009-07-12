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

#include "audio_p.h"

#include <QThread>

#include <grim/tools/idgenerator.h>




namespace Grim {




class AudioFormatFile;




class AudioBufferLoader : public QThread
{
	Q_OBJECT

public:
	struct Result
	{
		bool hasError;
		AudioFormatFile * file;

		AudioBufferContent content;
	};

	struct Request
	{
		int requestId;
		bool isCancelled;
		bool isPrioritized;
		bool isRunning;

		QString fileName;
		QByteArray format;
		AudioFormatFile * file;
		qint64 sampleOffset;
		qint64 sampleCount;
		bool closeFile;
	};

	AudioBufferLoader( QObject * parent = 0 );
	~AudioBufferLoader();

	void abort();

	int addRequest( const QString & fileName, const QByteArray & format,
		qint64 sampleOffset, qint64 sampleCount, bool closeFile, bool isPrioritized );
	int addRequest( AudioFormatFile * file,
		qint64 sampleOffset, qint64 sampleCount, bool closeFile, bool isPrioritized );
	void cancelRequest( int requestId );

	void increasePriority( int requestId );

signals:
	void requestFinished( int requestId, void * resultp );

protected:
	void run();

private slots:
	void _threadRequestFinished( int requestId, void * resultp );

private:
	void _addRequest( const Request & request );
	void _removeRequest( int requestId );
	bool _processRequest( Request * request, Result * result );

private:
	bool isAborted_;

	IdGenerator idGenerator_;

	QReadWriteLock requestsMutex_;
	QHash<int,Request> requestForId_;
	QList<int> prioritizedRequests_;
	QList<int> requests_;

	QMutex moreRequestsMutex_;
	bool isWaitingForMoreRequests_;
	QWaitCondition moreRequestsWaiter_;
};




} // namespace Grim
