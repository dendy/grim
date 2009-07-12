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

#include <QDialog>
#include <QRunnable>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QBasicTimer>
#include <QTime>

#include "ui_archivetest.h"

#include <grim/tools/speedcounter.h>




class QThreadPool;

class MainWindow;
class ArchiveTest;
class ArchiveTestWorker;




class ArchiveTestWorkerEvent : public QEvent
{
public:
	ArchiveTestWorkerEvent( ArchiveTestWorker * _worker, int eventType );

	ArchiveTestWorker * worker;
};




class ArchiveTestWorkerProcessedEvent : public ArchiveTestWorkerEvent
{
public:
	ArchiveTestWorkerProcessedEvent( ArchiveTestWorker * _worker,
		int _doneCount, int _failedCount, qint64 _bytesReaded );

	int doneCount;
	int failedCount;
	qint64 bytesReaded;
};




class ArchiveTestWorkerFinishedEvent : public ArchiveTestWorkerEvent
{
public:
	ArchiveTestWorkerFinishedEvent( ArchiveTestWorker * _worker );
};




class ArchiveTestWorker : public QRunnable
{
public:
	ArchiveTestWorker( const QStringList & dirPaths, ArchiveTest * archiveTest );

	void abort();

	void addDirPath( const QString & dirPath );
	void removeDirPath( const QString & dirPath );

	void run();

private:
	QStringList _collectFileNames( const QString & dirPath ) const;
	void _body();

private:
	ArchiveTest * archiveTest_;

	QReadWriteLock mutex_;
	QWaitCondition waiter_;
	bool isAborted_;
	QStringList addedDirPaths_;
	QStringList removedDirPaths_;
	ArchiveTestWorkerEvent * pendingEvent_;

	int totalFileNames_;
	QStringList dirPaths_;
	QList<QStringList> fileNames_;

	QByteArray buffer_;

	friend class ArchiveTest;
};




class ArchiveTest : public QDialog
{
	Q_OBJECT

public:
	enum EventType
	{
		EventType_Processed = QEvent::User + 1,
		EventType_Finished
	};

	ArchiveTest( MainWindow * mainWindow );
	~ArchiveTest();

	void addDirPath( const QString & dirPath );
	void removeDirPath( const QString & dirPath );

	void retranslateUi();

	bool isStarted() const;
	void abort();

protected:
	bool event( QEvent * e );
	void timerEvent( QTimerEvent * e );
	void changeEvent( QEvent * e );

private slots:
	void on_buttonStart_clicked();
	void on_buttonStop_clicked();
	void on_spinBoxWorkersCount_valueChanged( int value );

private:
	void _addWorker();
	void _removeWorker();

	void _updateButtons();
	void _updateLabelsByTimer();
	void _updateLabels();

private:
	Ui_ArchiveTest ui_;

	MainWindow * mainWindow_;

	QThreadPool * workersPool_;
	QList<ArchiveTestWorker*> workers_;
	ArchiveTestWorker * currentlyAbortingWorker_;

	int totalFilesProcessed_;
	int filesDoneCount_;
	int filesFailedCount_;
	qint64 totalBytesReaded_;

	QBasicTimer speedTimer_;
	QTime speedTime_;
	Grim::IntSpeedCounter filesSpeedCounter_;
	Grim::IntSpeedCounter bytesSpeedCounter_;

	QBasicTimer updateLabelsTimer_;
};




inline ArchiveTestWorkerEvent::ArchiveTestWorkerEvent( ArchiveTestWorker * _worker, int eventType ) :
	QEvent( (QEvent::Type)eventType ), worker( _worker )
{}




inline ArchiveTestWorkerProcessedEvent::ArchiveTestWorkerProcessedEvent( ArchiveTestWorker * _worker,
	int _doneCount, int _failedCount, qint64 _bytesReaded ) :
	ArchiveTestWorkerEvent( _worker, ArchiveTest::EventType_Processed ),
	doneCount( _doneCount ),
	failedCount( _failedCount ),
	bytesReaded( _bytesReaded )
{}




inline ArchiveTestWorkerFinishedEvent::ArchiveTestWorkerFinishedEvent( ArchiveTestWorker * _worker ) :
	ArchiveTestWorkerEvent( _worker, ArchiveTest::EventType_Finished )
{}




inline bool ArchiveTest::isStarted() const
{ return workersPool_ != 0; }
