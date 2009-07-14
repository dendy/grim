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

#include "archivetest.h"

#include <QDirIterator>
#include <QThreadPool>

#include <grim/tools/utils.h>

#include "mainwindow.h"




static const int DefaultWorkersCount = 4;




ArchiveTestWorker::ArchiveTestWorker( const QStringList & dirPaths, ArchiveTest * archiveTest ) :
	archiveTest_( archiveTest ), isAborted_( false ), addedDirPaths_( dirPaths ), pendingEvent_( 0 )
{
}


void ArchiveTestWorker::abort()
{
	QWriteLocker locker( &mutex_ );

	isAborted_ = true;

	if ( pendingEvent_ )
	{
		pendingEvent_->worker = 0;
		pendingEvent_ = 0;
	}

	waiter_.wakeOne();
}


void ArchiveTestWorker::addDirPath( const QString & dirPath )
{
	QWriteLocker locker( &mutex_ );

	if ( removedDirPaths_.contains( dirPath ) )
		removedDirPaths_.removeOne( dirPath );
	else
		addedDirPaths_ << dirPath;

	waiter_.wakeOne();
}


void ArchiveTestWorker::removeDirPath( const QString & dirPath )
{
	QWriteLocker locker( &mutex_ );

	if ( addedDirPaths_.contains( dirPath ) )
		addedDirPaths_.removeOne( dirPath );
	else
		removedDirPaths_ << dirPath;

	waiter_.wakeOne();
}


void ArchiveTestWorker::run()
{
	totalFileNames_ = 0;

	_body();

	{
		QWriteLocker locker( &mutex_ );

		if ( isAborted_ )
			return;

		QCoreApplication::postEvent( archiveTest_, pendingEvent_ = new ArchiveTestWorkerFinishedEvent( this ) );
		waiter_.wait( &mutex_ );
		pendingEvent_ = 0;
	}
}


QStringList ArchiveTestWorker::_collectFileNames( const QString & dirPath ) const
{
	QStringList list;

	for ( QDirIterator it( dirPath, QDirIterator::Subdirectories ); it.hasNext(); )
	{
		it.next();

		if ( isAborted_ )
			return QStringList();

		const QFileInfo fileInfo = it.fileInfo();

		if ( !fileInfo.isFile() )
			continue;

		list << fileInfo.absoluteFilePath();
	}

	return list;
}


void ArchiveTestWorker::_body()
{
	buffer_.resize( 1024*1024 );

	// enter infinite loop until not aborted
	while ( true )
	{
		{
			QStringList dirPathsToCollect;

			{
				QWriteLocker locker( &mutex_ );

				if ( isAborted_ )
					return;

				for ( QStringListIterator removedDirsIt( removedDirPaths_ ); removedDirsIt.hasNext(); )
				{
					const QString dirPathToRemove = removedDirsIt.next();
					const int index = dirPaths_.indexOf( dirPathToRemove );
					Q_ASSERT( index != -1 );

					dirPaths_.removeAt( index );

					QStringList fileNames = fileNames_.takeAt( index );
					totalFileNames_ -= fileNames.count();
					Q_ASSERT( totalFileNames_ >= 0 );
				}

				for ( QStringListIterator addedDirsIt( addedDirPaths_ ); addedDirsIt.hasNext(); )
					dirPathsToCollect << addedDirsIt.next();

				addedDirPaths_.clear();
				removedDirPaths_.clear();
			}

			while ( !dirPathsToCollect.isEmpty() )
			{
				const QString dirPath = dirPathsToCollect.takeFirst();
				const QStringList fileNames = _collectFileNames( dirPath );

				dirPaths_ << dirPath;
				fileNames_ << fileNames;
				totalFileNames_ += fileNames.count();

				QWriteLocker locker( &mutex_ );
				if ( isAborted_ )
					return;
			}
		}

		if ( totalFileNames_ == 0 )
		{
			// nothing collected, will wait for more
			QWriteLocker locker( &mutex_ );
			if ( isAborted_ )
				return;
			waiter_.wait( &mutex_ );
			continue;
		}

		QList<QFile*> files;

		{
			const int filesCount = 1 + rand() % 4;

			for ( int i = 0; i < filesCount; ++i )
			{
				const int index = rand() % totalFileNames_;

				int decIndex = index;
				QString fileName;
				bool found = false;
				for ( QListIterator<QStringList> it( fileNames_ ); it.hasNext(); )
				{
					const QStringList & fileNames = it.next();
					if ( decIndex < fileNames.count() )
					{
						fileName = fileNames.at( decIndex );
						found = true;
						break;
					}
					decIndex -= fileNames.count();
				}
				Q_ASSERT( found );

				QFile * file = new QFile( fileName );
				if ( !file->open( QIODevice::ReadOnly ) )
				{
					delete file;
					QCoreApplication::postEvent( archiveTest_,
						new ArchiveTestWorkerProcessedEvent( this, 0, 1, 0 ) );
					continue;
				}

				files << file;
			}
		}

		while ( !files.isEmpty() )
		{
			{
				QWriteLocker locker( &mutex_ );
				if ( isAborted_ )
				{
					qDeleteAll( files );
					return;
				}
			}

			int doneCount = 0;
			int failedCount = 0;
			qint64 bytesReaded = 0;

			for ( QMutableListIterator<QFile*> fileIt( files ); fileIt.hasNext(); )
			{
				QFile * file = fileIt.next();

				const qint64 bytes = file->read( buffer_.data(), buffer_.size() );

				if ( bytes == -1 )
				{
					delete file;
					fileIt.remove();
					failedCount++;
					continue;
				}

				if ( bytes == 0 )
				{
					delete file;
					fileIt.remove();
					doneCount++;
					continue;
				}

				bytesReaded += bytes;
			}

			QCoreApplication::postEvent( archiveTest_,
				new ArchiveTestWorkerProcessedEvent( this, doneCount, failedCount, bytesReaded ) );
		}

		{
			QWriteLocker locker( &mutex_ );
			if ( isAborted_ )
				return;

//			QCoreApplication::postEvent( archiveTest_,
//				pendingEvent_ = new ArchiveTestWorkerProcessedEvent( this, doneCount, failedCount, bytesReaded ) );
//			waiter_.wait( &mutex_ );
//			pendingEvent_ = 0;

			if ( isAborted_ )
				return;
		}
	}
}




ArchiveTest::ArchiveTest( MainWindow * mainWindow ) :
	QDialog( mainWindow ),
	mainWindow_( mainWindow )
{
	ui_.setupUi( this );

	workersPool_ = 0;

	ui_.spinBoxWorkersCount->setValue( DefaultWorkersCount );

	filesSpeedCounter_.setTimeout( 1000 );
	bytesSpeedCounter_.setTimeout( 1000 );

	retranslateUi();

	_updateButtons();
}


ArchiveTest::~ArchiveTest()
{
	abort();
}


void ArchiveTest::abort()
{
	if ( !isStarted() )
		return;

	for ( QListIterator<ArchiveTestWorker*> workersIt( workers_ ); workersIt.hasNext(); )
	{
		ArchiveTestWorker * worker = workersIt.next();
		worker->abort();
	}

	QCoreApplication::removePostedEvents( this, EventType_Processed );
	QCoreApplication::removePostedEvents( this, EventType_Finished );

	workersPool_->waitForDone();
	delete workersPool_;
	workersPool_ = 0;

	workers_.clear();

	speedTimer_.stop();

	_updateButtons();
}


void ArchiveTest::addDirPath( const QString & dirPath )
{
	if ( !isStarted() )
		return;

	for ( QListIterator<ArchiveTestWorker*> it( workers_ ); it.hasNext(); )
		it.next()->addDirPath( dirPath );
}


void ArchiveTest::removeDirPath( const QString & dirPath )
{
	if ( !isStarted() )
		return;

	for ( QListIterator<ArchiveTestWorker*> it( workers_ ); it.hasNext(); )
		it.next()->removeDirPath( dirPath );
}


void ArchiveTest::retranslateUi()
{
	setWindowTitle( QString( "%1 - %2" )
		.arg( tr( "Archive Test" ) )
		.arg( QCoreApplication::applicationName() ) );
}


bool ArchiveTest::event( QEvent * e )
{
	if ( e->type() == (QEvent::Type)EventType_Processed || e->type() == (QEvent::Type)EventType_Finished )
	{
		ArchiveTestWorkerEvent * we = static_cast<ArchiveTestWorkerEvent*>( e );
		if ( !we->worker )
		{
			// this event was canceled by abort call earlier
			return true;
		}

		if ( we->worker == currentlyAbortingWorker_ )
		{
			// ignore this event, also we->worker already could point to garbage
			return true;
		}

		{
			QWriteLocker locker( &we->worker->mutex_ );
			we->worker->waiter_.wakeOne();
		}

		switch ( e->type() )
		{
		case EventType_Processed:
		{
			ArchiveTestWorkerProcessedEvent * pe = static_cast<ArchiveTestWorkerProcessedEvent*>( we );

			totalFilesProcessed_ += pe->doneCount + pe->failedCount;
			filesDoneCount_ += pe->doneCount;
			filesFailedCount_ += pe->failedCount;
			totalBytesReaded_ += pe->bytesReaded;

			const int msecs = speedTime_.restart();
			filesSpeedCounter_.hit( pe->doneCount + pe->failedCount, msecs );
			bytesSpeedCounter_.hit( pe->bytesReaded, msecs );

			_updateLabelsByTimer();
		}
			break;

		case EventType_Finished:
		{
			ArchiveTestWorkerFinishedEvent * fe = static_cast<ArchiveTestWorkerFinishedEvent*>( we );
			workers_.removeOne( fe->worker );
		}
			break;
		}

		return true;
	}

	return QDialog::event( e );
}


void ArchiveTest::timerEvent( QTimerEvent * e )
{
	if ( e->timerId() == speedTimer_.timerId() )
	{
		const int msecs = speedTime_.restart();
		filesSpeedCounter_.hit( 0, msecs );
		bytesSpeedCounter_.hit( 0, msecs );

		_updateLabels();

		return;
	}

	if ( e->timerId() == updateLabelsTimer_.timerId() )
	{
		updateLabelsTimer_.stop();
		_updateLabels();
		return;
	}

	return QDialog::timerEvent( e );
}


void ArchiveTest::changeEvent( QEvent * e )
{
	if ( e->type() == QEvent::LanguageChange )
		retranslateUi();

	QWidget::changeEvent( e );
}


void ArchiveTest::on_buttonStart_clicked()
{
	Q_ASSERT( !isStarted() );

	totalFilesProcessed_ = 0;
	filesDoneCount_ = 0;
	filesFailedCount_ = 0;
	totalBytesReaded_ = 0;

	speedTimer_.start( 100, this );
	speedTime_.start();
	filesSpeedCounter_.reset();
	bytesSpeedCounter_.reset();

	workersPool_ = new QThreadPool( this );

	currentlyAbortingWorker_ = 0;

	updateLabelsTimer_.stop();

	const int workersCount = ui_.spinBoxWorkersCount->value();
	for ( int i = 0; i < workersCount; ++i )
		_addWorker();

	_updateButtons();
	_updateLabels();
}


void ArchiveTest::on_buttonStop_clicked()
{
	Q_ASSERT( isStarted() );

	abort();
}


void ArchiveTest::on_spinBoxWorkersCount_valueChanged( int value )
{
	if ( !isStarted() )
		return;

	if ( value == workers_.count() )
		return;

	while ( value < workers_.count() )
		_removeWorker();

	while ( value > workers_.count() )
		_addWorker();
}


void ArchiveTest::_addWorker()
{
	ArchiveTestWorker * worker = new ArchiveTestWorker( mainWindow_->mountPoints(), this );

	workers_ << worker;

	workersPool_->setMaxThreadCount( workers_.count() );
	workersPool_->start( worker );
}


void ArchiveTest::_removeWorker()
{
	ArchiveTestWorker * worker = workers_.takeFirst();
	worker->abort();

	currentlyAbortingWorker_ = worker;
	QCoreApplication::sendPostedEvents( this, EventType_Processed );
	currentlyAbortingWorker_ = 0;
}


void ArchiveTest::_updateButtons()
{
	ui_.buttonStart->setEnabled( !isStarted() );
	ui_.buttonStop->setEnabled( isStarted() );
}


void ArchiveTest::_updateLabelsByTimer()
{
	if ( !updateLabelsTimer_.isActive() )
		updateLabelsTimer_.start( 0, this );
}


void ArchiveTest::_updateLabels()
{
	ui_.labelTotalFilesProcessed->setText( QString::number( totalFilesProcessed_ ) );
	ui_.labelFilesDone->setText( QString::number( filesDoneCount_ ) );
	ui_.labelFilesFailed->setText( QString::number( filesFailedCount_ ) );

	ui_.labelTotalBytesReaded->setText( Grim::Utils::convertBytesToString( totalBytesReaded_, 2 ) );

	ui_.labelFilesSpeed->setText( tr( "%1" ).arg( filesSpeedCounter_.value() * 1000, 0, 'f', 2 ) );
	ui_.labelBytesSpeed->setText( tr( "%1 per second" )
		.arg( Grim::Utils::convertBytesToString( bytesSpeedCounter_.value() * 1000, 2 ) ) );
}
