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

#include "archive_p.h"

#include "archivemanager.h"
#include "archivemanager_p.h"

#include <QPointer>
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_WS_WIN
#include <objbase.h>
#endif




namespace Grim {




/** \internal
 * Low-level WinAPI converter from long paths to short ones.
 */
extern QString toShortPath( const QString & path )
{
#ifdef Q_WS_WIN
	QString ret;
	int size;
	QT_WA({
		wchar_t tempPath[MAX_PATH];
		size = GetShortPathNameW((LPCWSTR)path.data(),tempPath,MAX_PATH);
		ret = QString::fromUtf16( (ushort*)tempPath, size );
	} , {
		QByteArray pathLocal = path.toLocal8Bit();
		char tempPath[MAX_PATH];
		size = GetShortPathNameA(pathLocal.constData(),tempPath,MAX_PATH);
		ret = QString::fromLocal8Bit( tempPath, size );
	});
	return ret;
#else
	return path;
#endif
}


/** \internal
 * Low-level WinAPI converter from short paths to long ones.
 */
extern QString toLongPathInternal( const QString & path )
{
#ifdef Q_WS_WIN
	QString ret;
	int size;
	QT_WA({
		wchar_t tempPath[MAX_PATH];
		size = GetLongPathNameW( (LPCWSTR)path.data(), tempPath, MAX_PATH );
		ret = QString::fromUtf16( (ushort*)tempPath, size );
	} , {
		QByteArray pathLocal = path.toLocal8Bit();
		char tempPath[MAX_PATH];
		size = GetLongPathNameA(pathLocal.constData(),tempPath,MAX_PATH);
		ret = QString::fromLocal8Bit( tempPath, size );
	});
	return ret;
#else
	return path;
#endif
}


/** \internal
 * Windows only.
 * Converts as long as possible left part of the given \a path to long.
 * The rest unconverted right part just appended to converted one.
 * The result returned.
 */
extern QString toLongPath( const QString & path )
{
	// try to convert whole path at once
	QString longPath = toLongPathInternal( path );
	if ( !longPath.isEmpty() )
		return longPath;

	// WinAPI dislikes whole path, try to pass less shorter and shorter path,
	// by cutting last portion from right after slash /

	int slashPos = path.length() - 1;
	while ( slashPos != -1 )
	{
		slashPos = path.lastIndexOf( QLatin1Char( '/' ), slashPos );

		if ( slashPos == -1 )
			break;

		QString leftPath = path.left( slashPos );
		QString longLeftPath = toLongPathInternal( leftPath );
		if ( !longLeftPath.isEmpty() )
			return longLeftPath + path.mid( slashPos );

		slashPos--;
	}

	return path;
}


/** \internal
 *
 * Returns suitable for the given \a path variant for case-sensitive comparison.
 * Under Linux/OSX this will be the given \a path itself.
 * Under Windows return lowered path.
 *
 * The reason on this function is to generate path for case-sensitive comparison,
 * because it is quicker than case-insensitive one.
 */
extern QString softToHardCleanPath( const QString & path )
{
#ifdef Q_WS_WIN
	return path.toLower();
#endif
	return path;
}


/** \internal
 * Returns clean full path for case-sensitive comparison.
 */
extern QString toSoftCleanPath( const QString & path )
{
	return QDir::cleanPath( toLongPath( path ) );
}




static const int UpdateInterval = 1000; // interval for updating non locked archive




// archive signatures
static const quint32 LocalFileHeaderSignature       = 0x04034b50;
static const quint32 ExtraDataSignature             = 0x08064b50;
static const quint32 CentralFileHeaderSignature     = 0x02014b50;
static const quint32 DigitalSignatureSignature      = 0x05054b50;
static const quint32 EndOfCentralDirectorySignature = 0x06054b50;

static const int EndOfCentralDirectorySize = 18;




// data struct of local file header in archive
struct LocalFileHeaderStruct
{
	quint16 versionToExtract;
	quint16 bitFlag;
	quint16 compressionMethod;
	quint16 modTime;
	quint16 modDate;
	quint32 crc32;
	quint32 compressedSize;
	quint32 uncompressedSize;

	QString fileName;
	QByteArray extraField;
};


inline QDataStream & operator<<( QDataStream & ds, const LocalFileHeaderStruct & s )
{
	ds << LocalFileHeaderSignature;

	const QByteArray fileNameEncoded = s.fileName.toUtf8();

	ds << s.versionToExtract;
	ds << s.bitFlag;
	ds << s.compressionMethod;
	ds << s.modTime;
	ds << s.modDate;
	ds << s.crc32;
	ds << s.compressedSize;
	ds << s.uncompressedSize;

	ds << (quint16)fileNameEncoded.size();
	ds << (quint16)s.extraField.size();

	ds.writeRawData( fileNameEncoded.constData(), fileNameEncoded.size() );
	ds.writeRawData( s.extraField.constData(), s.extraField.size() );

	return ds;
}


inline QDataStream & operator>>( QDataStream & ds, LocalFileHeaderStruct & s )
{
	quint32 signature = 0;
	ds >> signature;
	if ( signature != LocalFileHeaderSignature )
	{
		ds.setStatus( QDataStream::ReadCorruptData );
		return ds;
	}

	quint16 fileNameSize;
	quint16 extraFieldSize;

	ds >> s.versionToExtract;
	ds >> s.bitFlag;
	ds >> s.compressionMethod;
	ds >> s.modTime;
	ds >> s.modDate;
	ds >> s.crc32;
	ds >> s.compressedSize;
	ds >> s.uncompressedSize;
	ds >> fileNameSize;
	ds >> extraFieldSize;

	if ( ds.status() != ds.Ok )
		return ds;

	QByteArray fileNameEncoded;
	fileNameEncoded.resize( fileNameSize );
	ds.readRawData( fileNameEncoded.data(), fileNameSize );
	s.fileName = QString::fromUtf8( fileNameEncoded );

	s.extraField.resize( extraFieldSize );
	ds.readRawData( s.extraField.data(), extraFieldSize );

	return ds;
}




/** \internal
 * \class DummyLocalFileHeaderStruct
 * Stub to skip LocalFileHeaderStruct from QDataStream without allocating memory
 */

struct DummyLocalFileHeaderStruct
{};


inline QDataStream & operator>>( QDataStream & ds, DummyLocalFileHeaderStruct & s )
{
	quint32 signature = 0;
	ds >> signature;
	if ( signature != LocalFileHeaderSignature )
	{
		ds.setStatus( QDataStream::ReadCorruptData );
		return ds;
	}

	quint16 fileNameSize;
	quint16 extraFieldSize;

	// 22 is a size of struct without file name and extra field lengths
	ds.device()->seek( ds.device()->pos() + 22 );

	ds >> fileNameSize;
	ds >> extraFieldSize;

	if ( ds.status() != ds.Ok )
		return ds;

	ds.device()->seek( ds.device()->pos() + fileNameSize + extraFieldSize );

	return ds;
}





// data struct of data descriptor
struct DataDescriptorStruct
{
	quint32 crc32;
	quint32 compressedSize;
	quint32 uncompressedSize;
};


inline QDataStream & operator<<( QDataStream & ds, const DataDescriptorStruct & s )
{
	ds << s.crc32;
	ds << s.compressedSize;
	ds << s.uncompressedSize;

	return ds;
}


inline QDataStream & operator>>( QDataStream & ds, DataDescriptorStruct & s )
{
	ds >> s.crc32;
	ds >> s.compressedSize;
	ds >> s.uncompressedSize;

	return ds;
}




// data struct of archive extra field
struct ExtraDataStruct
{
	QByteArray extraField;
};


inline QDataStream & operator<<( QDataStream & ds, const ExtraDataStruct & s )
{
	ds << ExtraDataSignature;

	ds << (quint32)s.extraField.size();
	ds.writeRawData( s.extraField.constData(), s.extraField.size() );

	return ds;
}


inline QDataStream & operator>>( QDataStream & ds, ExtraDataStruct & s )
{
	quint32 signature = 0;
	ds >> signature;
	if ( signature != ExtraDataSignature )
	{
		ds.setStatus( QDataStream::ReadCorruptData );
		return ds;
	}

	quint32 extraFieldSize;
	ds >> extraFieldSize;

	if ( ds.status() != QDataStream::Ok )
		return ds;

	s.extraField.resize( extraFieldSize );
	ds.readRawData( s.extraField.data(), extraFieldSize );

	return ds;
}




// data struct of file header in central directory
struct FileHeaderStruct
{
	quint16 versionMadeBy;
	quint16 versionNeedToExtract;
	quint16 bitFlag;
	quint16 compressionMethod;
	quint16 modTime;
	quint16 modDate;
	quint32 crc32;
	quint32 compressedSize;
	quint32 uncompressedSize;
	quint16 diskNumberStart;
	quint16 internalFileAttributes;
	quint32 externalFileAttributes;
	quint32 localHeaderOffset;

	QString fileName;
	QByteArray extraField;
	QString fileComment;
};


inline QDataStream & operator<<( QDataStream & ds, const FileHeaderStruct & s )
{
	ds << CentralFileHeaderSignature;

	const QByteArray fileNameEncoded = s.fileName.toUtf8();
	const QByteArray fileCommentEncoded = s.fileComment.toUtf8();

	ds << s.versionMadeBy;
	ds << s.versionNeedToExtract;
	ds << s.bitFlag;
	ds << s.compressionMethod;
	ds << s.modTime;
	ds << s.modDate;
	ds << s.crc32;
	ds << s.compressedSize;
	ds << s.uncompressedSize;
	ds << (quint16)fileNameEncoded.size();
	ds << (quint16)s.extraField.size();
	ds << (quint16)fileCommentEncoded.size();
	ds << s.diskNumberStart;
	ds << s.internalFileAttributes;
	ds << s.externalFileAttributes;
	ds << s.localHeaderOffset;

	ds.writeRawData( fileNameEncoded.constData(), fileNameEncoded.size() );
	ds.writeRawData( s.extraField.constData(), s.extraField.size() );
	ds.writeRawData( fileCommentEncoded.constData(), fileCommentEncoded.size() );

	return ds;
}


inline QDataStream & operator>>( QDataStream & ds, FileHeaderStruct & s )
{
	quint32 signature = 0;
	ds >> signature;
	if ( signature != CentralFileHeaderSignature )
	{
		ds.setStatus( QDataStream::ReadCorruptData );
		return ds;
	}

	quint16 fileNameSize;
	quint16 extraFieldSize;
	quint16 fileCommentSize;

	ds >> s.versionMadeBy;
	ds >> s.versionNeedToExtract;
	ds >> s.bitFlag;
	ds >> s.compressionMethod;
	ds >> s.modTime;
	ds >> s.modDate;
	ds >> s.crc32;
	ds >> s.compressedSize;
	ds >> s.uncompressedSize;
	ds >> fileNameSize;
	ds >> extraFieldSize;
	ds >> fileCommentSize;
	ds >> s.diskNumberStart;
	ds >> s.internalFileAttributes;
	ds >> s.externalFileAttributes;
	ds >> s.localHeaderOffset;

	if ( ds.status() != QDataStream::Ok )
		return ds;

	QByteArray fileNameEncoded;
	fileNameEncoded.resize( fileNameSize );
	ds.readRawData( fileNameEncoded.data(), fileNameSize );
	s.fileName = QString::fromUtf8( fileNameEncoded );

	s.extraField.resize( extraFieldSize );
	ds.readRawData( s.extraField.data(), extraFieldSize );

	QByteArray fileCommentEncoded;
	fileCommentEncoded.resize( fileCommentSize );
	ds.readRawData( fileCommentEncoded.data(), fileCommentSize );
	s.fileComment = QString::fromUtf8( fileCommentEncoded );

	return ds;
}




// data struct of digital signature
struct DigitalSignatureStruct
{
	QByteArray data;
};


inline QDataStream & operator<<( QDataStream & ds, const DigitalSignatureStruct & s )
{
	ds << DigitalSignatureSignature;

	ds << (quint16)s.data.size();
	ds.writeRawData( s.data.constData(), s.data.size() );

	return ds;
}


inline QDataStream & operator>>( QDataStream & ds, DigitalSignatureStruct & s )
{
	quint32 signature = 0;
	ds >> signature;
	if ( signature != DigitalSignatureSignature )
	{
		ds.setStatus( QDataStream::ReadCorruptData );
		return ds;
	}

	quint16 dataSize;

	ds >> dataSize;

	if ( ds.status() != QDataStream::Ok )
		return ds;

	s.data.resize( dataSize );
	ds.readRawData( s.data.data(), dataSize );

	return ds;
}




// data struct of end of central directory record
struct EndOfCentralDirectoryStruct
{
	quint16 numberOfThisDisk;
	quint16 numberOfTheStartDisk;
	quint16 numberOfEntriesOnThisDisk;
	quint16 numberOfEntriesTotal;
	quint32 sizeOfTheCentralDirectory;
	quint32 offsetOfCentralDirectory;

	QString zipFileComment;
};


inline QDataStream & operator<<( QDataStream & ds, const EndOfCentralDirectoryStruct & s )
{
	ds << EndOfCentralDirectorySignature;

	const QByteArray zipFileCommentEncoded = s.zipFileComment.toUtf8();

	ds << s.numberOfThisDisk;
	ds << s.numberOfTheStartDisk;
	ds << s.numberOfEntriesOnThisDisk;
	ds << s.numberOfEntriesTotal;
	ds << s.sizeOfTheCentralDirectory;
	ds << s.offsetOfCentralDirectory;

	ds << (quint16)zipFileCommentEncoded.size();
	ds.writeRawData( zipFileCommentEncoded.constData(), zipFileCommentEncoded.size() );

	return ds;
}


inline QDataStream & operator>>( QDataStream & ds, EndOfCentralDirectoryStruct & s )
{
	quint32 signature = 0;
	ds >> signature;
	if ( signature != EndOfCentralDirectorySignature )
	{
		ds.setStatus( QDataStream::ReadCorruptData );
		return ds;
	}

	quint16 zipFileCommentSize;

	ds >> s.numberOfThisDisk;
	ds >> s.numberOfTheStartDisk;
	ds >> s.numberOfEntriesOnThisDisk;
	ds >> s.numberOfEntriesTotal;
	ds >> s.sizeOfTheCentralDirectory;
	ds >> s.offsetOfCentralDirectory;
	ds >> zipFileCommentSize;

	if ( ds.status() != QDataStream::Ok )
		return ds;

	QByteArray zipFileCommentEncoded;
	zipFileCommentEncoded.resize( zipFileCommentSize );
	ds.readRawData( zipFileCommentEncoded.data(), zipFileCommentSize );
	s.zipFileComment = QString::fromUtf8( zipFileCommentEncoded );

	return ds;
}




static QThreadStorage<ArchiveThreadCache*> _archiveThreadCache;


ArchiveThreadCache * archiveThreadCache()
{
	if ( !_archiveThreadCache.hasLocalData() )
		_archiveThreadCache.setLocalData( new ArchiveThreadCache );

	return _archiveThreadCache.localData();
}




/** \internal
 *
 * \class ArchiveInstance
 *
 * Thread-safe entity that allows safely lock and work with Archive pointer without worrying
 * that somebody will spontaneously destroy it.
 */

ArchiveInstance::ArchiveInstance() :
	d( ArchiveManagerPrivate::sharedManagerPrivate()->sharedNullArchiveInstanceData() )
{
}


bool ArchiveInstance::isNull() const
{
	return d == ArchiveManagerPrivate::sharedManagerPrivate()->sharedNullArchiveInstanceData();
}




/** \internal
 *
 * \class ArchiveWorker
 *
 * Dummy wrapper that just calls archivePrivate_->_workerBody().
 * Actually ArchivePrivate class could be derived from QThread by itself,
 * don't know what variant is better, leave as is.
 */

class ArchiveWorker : public QThread
{
public:
	ArchiveWorker( ArchivePrivate * archivePrivate ) :
		archivePrivate_( archivePrivate )
	{}

protected:
	void run()
	{
		archivePrivate_->_workerBody();
	}

public:
	ArchivePrivate * archivePrivate_;
};





/** \internal
 *
 * \class ArchivePrivate
 *
 * Actual working horse for the Archive instance.
 */

ArchivePrivate::ArchivePrivate( Archive * archive ) :
	archiveInstance_( new ArchiveInstanceData( this ) ),
	archive_( archive ),
	state_( Archive::State_Idle ),
	isBroken_( false ),
	openMode_( Grim::Archive::NotOpen ),
	isInitialized_( false ),
	worker_( 0 ),
	contentsMutex_( QReadWriteLock::Recursive )
{
	treatAsDir_ = true;

	updateInterval_ = UpdateInterval;
}


ArchivePrivate::~ArchivePrivate()
{
	archive_->blockSignals( true );

	close();
}


void ArchivePrivate::_makeCleanMountPointPath()
{
	// construct hard cleaned absolute path to archive's mount point
	// cleanMountPointPath_ can be used for case-sensitive check
	// whether the given path points into this archive or not
	cleanMountPointPath_ = mountPointAbsolutePath_.isEmpty() ? fileNameAbsolutePath_ : mountPointAbsolutePath_;
	cleanMountPointPath_ = toSoftCleanPath( cleanMountPointPath_ );
	cleanMountPointPath_ = softToHardCleanPath( cleanMountPointPath_ );
}


void ArchivePrivate::setFileName( const QString & fileName )
{
	if ( openMode_ != Grim::Archive::NotOpen )
	{
		qWarning( "Grim::ArchivePrivate::setFileName(): Archive is already opened." );
		return;
	}

	fileName_ = fileName;

	fileNameAbsolutePath_ = QDir::cleanPath( QFileInfo( fileName_ ).absoluteFilePath() );

	while ( fileNameAbsolutePath_.endsWith( QLatin1Char( '/' ) ) )
		fileNameAbsolutePath_.truncate( fileNameAbsolutePath_.length() - 1 );

	_makeCleanMountPointPath();
}


void ArchivePrivate::setMountPoint( const QString & mountPoint )
{
	if ( openMode_ != Grim::Archive::NotOpen )
	{
		qWarning( "Grim::ArchivePrivate::setMountPoint(): Archive is already opened." );
		return;
	}

	mountPoint_ = mountPoint;

	mountPointAbsolutePath_ = QDir::cleanPath( QFileInfo( mountPoint_ ).absoluteFilePath() );

	while ( mountPointAbsolutePath_.endsWith( QLatin1Char( '/' ) ) )
		mountPointAbsolutePath_.truncate( mountPointAbsolutePath_.length() - 1 );

	_makeCleanMountPointPath();
}


bool ArchivePrivate::open( Grim::Archive::OpenMode openMode )
{
	if ( openMode_ != Grim::Archive::NotOpen )
	{
		qWarning( "Grim::ArchivePrivate::open(): Archive is already opened." );
		return false;
	}

	if ( !(openMode & Grim::Archive::ReadOnly) && !(openMode & Grim::Archive::WriteOnly) )
	{
		qWarning( "Grim::ArchivePrivate::open() : openMode must contains ReadOnly or WriteOnly or both." );
		return false;
	}

	// currently only ReadOnly mode supported
	if ( openMode & Grim::Archive::WriteOnly )
	{
		qWarning( "Grim::ArchivePrivate::open() : Writing not supported." );
		return false;
	}

	if ( !ArchiveManagerPrivate::sharedManagerPrivate()->registerArchive( archiveInstance_ ) )
		return false;

	if ( !_openArchive( openMode ) )
	{
		ArchiveManagerPrivate::sharedManagerPrivate()->unregisterArchive( archiveInstance_ );
		return false;
	}

	{
		QWriteLocker initializationLocker( &initializationMutex_ );
		isInitialized_ = true;
	}

	return true;
}


bool ArchivePrivate::_openArchive( Grim::Archive::OpenMode openMode )
{
	isArchiveDirty_ = true;
	archiveLastModified_ = QDateTime();
	rootEntry_ = 0;

	// temporary disable self to construct QFile instance on archive file
	_setTemporaryDisabled( true );
	archiveFile_.setFileName( fileName_ );
	if ( !(openMode & Grim::Archive::DontLock) )
	{
		QIODevice::OpenMode flags = 0;
		if ( openMode & Grim::Archive::ReadOnly )
			flags |= QIODevice::ReadOnly;
		if ( openMode & Grim::Archive::WriteOnly )
			flags |= QIODevice::WriteOnly;
		bool opened = archiveFile_.open( flags );
		if ( !opened )
		{
			_setTemporaryDisabled( false );
			qWarning( "Grim::ArchivePrivate::open() : Failed to open archive in locked mode." );
			return false;
		}

		if ( archiveFile_.isSequential() )
		{
			archiveFile_.close();
			_setTemporaryDisabled( false );
			qWarning( "Grim::ArchivePrivate::open() : Cannot read sequential archive." );
			return false;
		}
	}
	_setTemporaryDisabled( false );

	// create root entry
	{
		QWriteLocker contentsLocker( &contentsMutex_ );
		rootEntry_ = new ArchiveEntry;
		rootEntry_->info.isDir = true;
		entryForFilePath_[ QLatin1String( "/" ) ] = rootEntry_;
	}

	if ( !worker_ )
		worker_ = new ArchiveWorker( this );

	isWorkerAborted_ = false;
	isInvoked_ = false;
	isWaitingForJob_ = false;

	isTimeToUpdate_ = true;

	isBroken_ = false;
	wasInitialUpdate_ = false;
	updateIntervalTime_ = QTime();

	openMode_ = openMode;

	// update archive file every UpdateInterval msecs in non-locked mode
	if ( openMode_ & Grim::Archive::DontLock )
		updateTimer_.start( UpdateInterval, this );

	if ( openMode_ & Grim::Archive::Block )
	{
		// block and wait while archive will not be initially updated
		QMutexLocker blockLocker( &blockMutex_ );
		worker_->start();
		blockWaiter_.wait( &blockMutex_ );

		if ( !(openMode_ & Archive::DontLock) && isBroken_ )
		{
			// we are in locking mode, but archive is broken
			// this means that we will never update it again
			close();
			return false;
		}

		_setState( Archive::State_Ready, isBroken_ );
	}
	else
	{
		// no need to wait, return and emit stateChanged() signal later
		// when initial update will be done
		worker_->start();
		_setState( Archive::State_Initializing, false );
	}

	return true;
}


void ArchivePrivate::close()
{
	if ( openMode_ == Grim::Archive::NotOpen )
		return;

	{
		QWriteLocker initializationLocker( &initializationMutex_ );
		isInitialized_ = false;
	}

	ArchiveManagerPrivate::sharedManagerPrivate()->unregisterArchive( archiveInstance_ );

	updateTimer_.stop();

	{
		ArchiveInstance copy = archiveInstance_;

		// construct new ArchiveInstance for self, which nobody refers to
		archiveInstance_ = ArchiveInstance( new ArchiveInstanceData( this ) );

		// void old ArchiveInstance, which could be referenced by ArchiveFile's
		// they will think that this archive is gone and will not be able to relink self again
		QWriteLocker selfLocker( &copy.d->mutex );
		copy.d->archive = 0;
	}

	// abort worker
	_abortWorker();
	delete worker_;
	worker_ = 0;

	// unlink opened files and clear contents so no one can link again
	{
		QWriteLocker contentsLocker( &contentsMutex_ );

		// locking second mutex is useless because contentMutex_ in write mode overlaps it
		// QWriteLocker linkedFileInstancesLocker( &linkedFileInstancesMutex_ );

		while ( !linkedFileInstances_.isEmpty() )
		{
			ArchiveFileInstance fileInstance = linkedFileInstances_.takeFirst();

			// check that file ignored unlinking due to nulled archive in archive instance
			if ( !fileInstance.d->file )
				continue;

			_cleanupOpenedFile( fileInstance.d->file );

			Q_ASSERT( fileInstance.d->file->entry_ );
			Q_ASSERT( fileInstance.d->file->entry_->fileInstances.contains( fileInstance ) );

			fileInstance.d->file->entry_ = 0;

			// skip this removeOne()
			// anyway we can't detect which file skipped unlinking to assert it
			// fileInstance.d->file->entry_->fileInstances.removeOne( fileInstance );
		}

		// destroy contents
		QList<ArchiveEntry*> entriesToDelete;
		entriesToDelete << rootEntry_;
		while ( !entriesToDelete.isEmpty() )
		{
			ArchiveEntry * entryToDelete = entriesToDelete.takeFirst();
			entriesToDelete << entryToDelete->entries;

			// skip this assert, we can't process it, because files have ability to skip unlink step
			// between selfLocker and contentsLocker
			// Q_ASSERT( entryToDelete->fileInstances.isEmpty() );

			delete entryToDelete;
		}

		entryForFilePath_.clear();
		rootEntry_ = 0;
	}

	// destroy self from pairs and clear pending requests
	{
		QWriteLocker fileInstancesLocker( &fileInstancesMutex_ );
		for ( QListIterator<ArchiveFileInstance> it( fileInstances_ ); it.hasNext(); )
		{
			const ArchiveFileInstance & fileInstance = it.next();

			QReadLocker fileLocker( &fileInstance.d->mutex );

			// check that file ignored removing due to nulled archive in archive instance
			if ( !fileInstance.d->file )
				continue;

			QWriteLocker fileRequestLocker( &fileInstance.d->file->requestMutex_ );
			if ( fileInstance.d->file->request_ )
			{
				{
					QWriteLocker jobLocker( &jobMutex_ );
					requests_.removeOne( fileInstance.d->file->request_ );
				}

				fileInstance.d->file->request_ = 0;
				fileInstance.d->file->requestWaiter_.wakeOne();
			}
		}
		fileInstances_.clear();
	}

	// ensure no new requests given
	{
		QWriteLocker jobLocker( &jobMutex_ );
		Q_ASSERT( requests_.isEmpty() );
	}

	// clear contents
	globalComment_ = QString();

	// close archive file if it was opened
	{
		_setTemporaryDisabled( true );
		if ( archiveFile_.openMode() != QIODevice::NotOpen )
			archiveFile_.close();
		_setTemporaryDisabled( false );
	}

	openMode_ = Grim::Archive::NotOpen;

	_setState( Archive::State_Idle, false );
}


Grim::Archive::OpenMode ArchivePrivate::openMode() const
{
	return openMode_;
}


bool ArchivePrivate::treatAsDir() const
{
	return treatAsDir_;
}


void ArchivePrivate::setTreatAsDir( bool set )
{
	treatAsDir_ = set;
}


QString ArchivePrivate::globalComment() const
{
	return globalComment_;
}


void ArchivePrivate::registerFile( ArchiveFile * file )
{
	{
		QWriteLocker fileInstancesLocker( &fileInstancesMutex_ );
		fileInstances_ << file->fileInstance_;
	}
}


void ArchivePrivate::unregisterFile( ArchiveFile * file )
{
	// ensure file is unlinked
	Q_ASSERT( !file->entry_ );

	{
		QWriteLocker fileInstancesLocker( &fileInstancesMutex_ );
		fileInstances_.removeOne( file->fileInstance_ );
	}
}


/**
 *  Links \a file with existing entry if suitable one exists.
 */
void ArchivePrivate::linkFile( ArchiveFile * file )
{
	// check if file is already linked
	if ( file->entry_ )
		return;

	{
		// it's safe to access openMode_ here
		if ( !(openMode_ & Grim::Archive::Block) )
		{
			// wait until archive will be updated first time
			QMutexLocker blockLocker( &blockMutex_ );
			if ( !wasInitialUpdate_ )
			{
				contentsMutex_.unlock();
				blockWaiter_.wait( &blockMutex_ );
				contentsMutex_.lockForRead();
			}
		}
	}

	ArchiveEntry * entry = entryForFilePath_.value( file->internalFileName_ );
	if ( !entry )
		return;

	if ( !entry->info.canRead )
	{
		// file is not normal ZIP archive
		return;
	}

	QWriteLocker linkedFileInstancesLocker( &linkedFileInstancesMutex_ );

	file->entry_ = entry;
	entry->fileInstances << file->fileInstance_;

	Q_ASSERT( !linkedFileInstances_.contains( file->fileInstance_ ) );
	linkedFileInstances_ << file->fileInstance_;
}


/**
 * Unlinks \a file from existing entry.
 * This method actually called only from file's destructor.
 * File instance never acts first to unlink self.
 */
void ArchivePrivate::unlinkFile( ArchiveFile * file )
{
	if ( !file->entry_ )
		return;

	QWriteLocker linkedFileInstancesLocker( &linkedFileInstancesMutex_ );

	// file must close self explicitly before unlinking
	Q_ASSERT( !openedFileInstances_.contains( file->fileInstance_ ) );

	Q_ASSERT( linkedFileInstances_.contains( file->fileInstance_ ) );
	linkedFileInstances_.removeOne( file->fileInstance_ );

	Q_ASSERT( file->entry_->fileInstances.contains( file->fileInstance_ ) );

	file->entry_->fileInstances.removeOne( file->fileInstance_ );
	file->entry_ = 0;
}


/**
 * Looks up and returns entry for the given \a filePath.
 */
ArchiveEntry * ArchivePrivate::entryForFilePath( const QString & filePath ) const
{
	archiveThreadCache()->isManagerDisabled = true;
	const QString absoluteFilePath = QFileInfo( filePath ).absoluteFilePath();
	archiveThreadCache()->isManagerDisabled = false;

	// construct clean absolute file path, with which we can compare path to our mount point
	const QString cleanSoftFilePath = toSoftCleanPath( absoluteFilePath );
	const QString cleanHardFilePath = softToHardCleanPath( cleanSoftFilePath );

	// check if left part of the given path starts with out mount point
	if ( !cleanHardFilePath.startsWith( cleanMountPointPath_ ) )
		return 0;

	// cut off mount point path from left and obtain internal path inside archive as result
	QString internalFileName = cleanSoftFilePath.mid( cleanMountPointPath_.length() + 1 );
	Q_ASSERT( !internalFileName.endsWith( QLatin1Char( '/' ) ) );
	Q_ASSERT( !internalFileName.startsWith( QLatin1Char( '/' ) ) );

	if ( internalFileName.isEmpty() )
	{
		// this is a root directory of archive, named '/'
		internalFileName = QLatin1String( "/" );
	}

	return entryForFilePath_.value( internalFileName );
}


/**
 * Appends file operation \a request and blocks until it will not be done.
 * Note that we are in random thread now, it is normal to block file thread.
 */
void ArchivePrivate::processFileRequest( ArchiveFileRequest * request )
{
	QWriteLocker fileRequestLocker( &request->file()->requestMutex_ );

	{
		QWriteLocker jobLocker( &jobMutex_ );
		requests_ << request;
		jobWaiter_.wakeOne();
	}

	contentsMutex_.unlock();

	request->file()->request_ = request;
	request->file()->requestWaiter_.wait( &request->file()->requestMutex_ );

	contentsMutex_.lockForRead();
}


/**
 * Changes state to the given \a state and broken flag to \a isBroken
 * and emits signal stateChanged().
 */
void ArchivePrivate::_setState( Archive::State state, bool isBroken )
{
	if ( state_ == state && isBroken_ == isBroken )
		return;

	state_ = state;
	isBroken_ = isBroken;

	emit archive_->stateChanged( state_ );
}


/**
 * Marks self as temporary disabled to pass file requests to other file engine handlers.
 */
void ArchivePrivate::_setTemporaryDisabled( bool set )
{
	if ( set )
		archiveThreadCache()->disabledArchives << archiveInstance_;
	else
		archiveThreadCache()->disabledArchives.removeOne( archiveInstance_ );
}


bool ArchivePrivate::event( QEvent * e )
{
	// event from worker thread about state and broken flag changes
	if ( e->type() == (QEvent::Type)EventType_StateChanged )
	{
		StateChangedEvent * stateChangedEvent = static_cast<StateChangedEvent*>( e );

		// somebody can kill us if it dislikes our signal, cruel world
		QPointer<Archive> self( archive_ );
		_setState( (Archive::State)stateChangedEvent->state, stateChangedEvent->isBroken );
		if ( !self )
			return true;

		// wake up worker thread that waits until event will be handled
		QWriteLocker invokationLocker( &invokationMutex_ );
		isInvoked_ = false;
		invokationWaiter_.wakeOne();

		return true;
	}

	return QObject::event( e );
}


void ArchivePrivate::timerEvent( QTimerEvent * e )
{
	if ( e->timerId() == updateTimer_.timerId() )
	{
		QWriteLocker jobLocker( &jobMutex_ );
		isTimeToUpdate_ = true;
		jobWaiter_.wakeOne();
		return;
	}

	return QObject::timerEvent( e );
}


void ArchivePrivate::_abortWorker()
{
	if ( !worker_ )
		return;

	{
		QWriteLocker invokationLocker( &invokationMutex_ );
		isWorkerAborted_ = true;
		if ( isInvoked_ )
		{
			// ignore all State_Changed events worker sent us and just wake up worker thread
			QCoreApplication::removePostedEvents( this, EventType_StateChanged );
			invokationWaiter_.wakeOne();
		}
	}

	// release job waiter
	{
		QWriteLocker jobLocker( &jobMutex_ );
		jobWaiter_.wakeOne();
	}

	worker_->wait();
}


void ArchivePrivate::_workerBody()
{
	while ( !isWorkerAborted_ )
	{
		QList<ArchiveFileRequest*> requestsCopy;
		bool isTimeToUpdate;

		{
			QWriteLocker jobLocker( &jobMutex_ );

			// collect requests and update flag into local variables
			requestsCopy = requests_;
			requests_.clear();

			isTimeToUpdate = isTimeToUpdate_;
			isTimeToUpdate_ = false;

			// check that we really have something to work on now
			if ( requestsCopy.isEmpty() && !isTimeToUpdate )
			{
				// no jobs, will wait for more
				isWaitingForJob_ = true;
				jobWaiter_.wait( &jobMutex_ );
				isWaitingForJob_ = false;

				if ( isWorkerAborted_ )
				{
					QMutexLocker blockLocker( &blockMutex_ );
					if ( !wasInitialUpdate_ )
						blockWaiter_.wakeAll();
					break;
				}

				// collect requests and update flag again
				// now they are valid and we can proceed
				requestsCopy = requests_;
				requests_.clear();

				isTimeToUpdate = isTimeToUpdate_;
				isTimeToUpdate_ = false;
			}
		}

		bool shouldOpen = false;
		bool shouldUpdate = false;
		bool storedWasInitialUpdate = wasInitialUpdate_;
		bool updatedSuccessfully = false;

		if ( openMode_ & Grim::Archive::DontLock )
		{
			if ( !requestsCopy.isEmpty() )
				shouldOpen = true;

			if ( openedFileInstances_.isEmpty() && updateIntervalTime_.elapsed() > updateInterval_ )
			{
				_setTemporaryDisabled( true );
				QFileInfo fileInfo( fileName_ );
				_setTemporaryDisabled( false );

				if ( archiveLastModified_ != fileInfo.lastModified() )
				{
					isArchiveDirty_ = true;
					shouldOpen = true;
					shouldUpdate = true;
				}

				updateIntervalTime_.restart();
			}
		}
		else
		{
			shouldUpdate = isArchiveDirty_;
		}

		// open archive either for update or processing file requests or both
		if ( shouldOpen && !archiveFile_.isOpen() )
		{
			// at this point archive file should be closed, so lets open it
			_setTemporaryDisabled( true );
			QIODevice::OpenMode flags = 0;
			if ( openMode_ & Grim::Archive::ReadOnly )
				flags |= QIODevice::ReadOnly;
			if ( openMode_ & Grim::Archive::WriteOnly )
				flags |= QIODevice::WriteOnly;
			bool opened = archiveFile_.open( flags );
			_setTemporaryDisabled( false );

			if ( !opened )
			{
				qWarning( "Grim::ArchivePrivate::_workerBody() : Failed to open archive in non locked mode." );
			}
			else if ( archiveFile_.isSequential() )
			{
				archiveFile_.close();
				qWarning( "Grim::ArchivePrivate::_workerBody() : Cannot read sequential archive." );
			}
		}

		// update archive contents if neccessary
		if ( shouldUpdate )
		{
			if ( !archiveFile_.isOpen() )
				updatedSuccessfully = false;
			else
				updatedSuccessfully = _updateArchive();

			QMutexLocker blockLocker( &blockMutex_ );
			if ( !wasInitialUpdate_ && openMode_ & Archive::Block )
			{
				// we are in blocking mode, set isBroken_ flag right here
				isBroken_ = !updatedSuccessfully;
			}

			wasInitialUpdate_ = true;
			blockWaiter_.wakeAll();
		}

		// check if we need to process requests for file operations
		_processFileRequests( requestsCopy );

		// close archive file if all file handlers were closed
		if ( (openMode_ & Grim::Archive::DontLock) && openedFileInstances_.isEmpty() && archiveFile_.isOpen() )
		{
			_setTemporaryDisabled( true );

			// reset last modified time while archive file is opened
			QFileInfo fileInfo( archiveFile_ );
			archiveLastModified_ = fileInfo.lastModified();

			archiveFile_.close();

			_setTemporaryDisabled( false );
		}

		// if state was changed during archive update - invoke method in archive's thread
		// to set changes synchronously
		{
			QWriteLocker invokationLocker( &invokationMutex_ );

			if ( isWorkerAborted_ )
				break;

			const bool wasBroken = isBroken_;
			const bool nowBroken = shouldUpdate ? !updatedSuccessfully : wasBroken;

			if ( (!storedWasInitialUpdate && wasInitialUpdate_) || wasBroken != nowBroken )
			{
				int state = Archive::State_Ready;
				bool isBroken = nowBroken;

				isInvoked_ = true;
				QCoreApplication::postEvent( this, new StateChangedEvent( state, isBroken ) );
				invokationWaiter_.wait( &invokationMutex_ );
			}
		}
	}
}


/**
 * Updates archive contents and kicks file instances that points to disappeared entries by unlinking them.
 */
bool ArchivePrivate::_updateArchive()
{
	QWriteLocker locker( &contentsMutex_ );

	if ( isWorkerAborted_ )
		return false;

	globalComment_ = QString();

	// here goes actual update
	const bool isLoaded = _loadCentralDirectory();

	if ( !isLoaded )
	{
		// broken archive, will stay dirty regardless at which mode it is opened, locked or not
		return false;
	}

	// destroy all disappeared entries since this update
	// this also unlinks file instances that are points to them

	rootEntry_->existedAfterUpdate = true;

	QList<ArchiveEntry*> entries;
	entries << rootEntry_;
	while ( !entries.isEmpty() )
	{
		ArchiveEntry * entry = entries.takeFirst();
		if ( !entry->existedAfterUpdate )
		{
			Q_ASSERT( entry->existedBeforeUpdate );

			// remove entry reference from parent
			entry->parentEntry->entryForName.remove( entry->info.fileName );
			entry->parentEntry->entries.removeOne( entry );

			// destroy entry and all its children and clean them all from entryForFilePath_
			QList<ArchiveEntry*> entriesToDelete;
			entriesToDelete << entry;
			while ( !entriesToDelete.isEmpty() )
			{
				ArchiveEntry * entryToDelete = entriesToDelete.takeFirst();
				entriesToDelete << entryToDelete->entries;

				entryForFilePath_.remove( entryToDelete->info.filePath );

				// unlink opened files
				for ( QListIterator<ArchiveFileInstance> it( entryToDelete->fileInstances ); it.hasNext(); )
				{
					const ArchiveFileInstance & fileInstance = it.next();

					Q_ASSERT( fileInstance.d->file );
					Q_ASSERT( fileInstance.d->file->entry_ );

					_cleanupOpenedFile( fileInstance.d->file );

					fileInstance.d->file->entry_ = 0;

					Q_ASSERT( linkedFileInstances_.contains( fileInstance ) );
					linkedFileInstances_.removeOne( fileInstance );

					QWriteLocker fileRequestLocker( &fileInstance.d->file->requestMutex_ );
					if ( fileInstance.d->file->request_ )
					{
						{
							QWriteLocker jobLocker( &jobMutex_ );
							requests_.removeOne( fileInstance.d->file->request_ );
						}

						fileInstance.d->file->request_ = 0;
						fileInstance.d->file->requestWaiter_.wakeOne();
					}
				}
				entryToDelete->fileInstances.clear();

				delete entryToDelete;
			}
		}
		else
		{
			// mark as false for the next update
			entry->existedBeforeUpdate = true;
			entry->existedAfterUpdate = false;
			entry->changedAfterUpdate = false;

			entries << entry->entries;
		}
	}

	isArchiveDirty_ = false;

	return true;
}


/**
 * Low-level Zip-archive parser, that extracts all entries.
 */
bool ArchivePrivate::_loadCentralDirectory()
{
	// Central Directory must started at:
	// file size - end header - comment length

	const int archiveFileSize = archiveFile_.size();

	QDataStream ds( &archiveFile_ );
	ds.setByteOrder( QDataStream::LittleEndian );

	EndOfCentralDirectoryStruct endOfCentralDirectory;

	// check if archive has no comment
	for ( int commentLength = 0; ; ++commentLength )
	{
		if ( !archiveFile_.seek( archiveFileSize - 4 - EndOfCentralDirectorySize - commentLength ) ) // 4 bytes for signature
			return false;

		ds >> endOfCentralDirectory;

		if ( ds.status() == QDataStream::Ok )
			break;
	}

	// save global archive comment
	globalComment_ = endOfCentralDirectory.zipFileComment;

	// now we know exact number or entries, so reserve buckets for file paths
	static const int MaxBuckets = 65536;
	entryForFilePath_.reserve( qMin<int>( endOfCentralDirectory.numberOfEntriesTotal, MaxBuckets ) );

	// collect file headers one by one
	if ( !archiveFile_.seek( endOfCentralDirectory.offsetOfCentralDirectory ) )
		return false;

	for ( int i = 0; i < endOfCentralDirectory.numberOfEntriesTotal; ++i )
	{
		FileHeaderStruct fileHeader;
		ds >> fileHeader;

		if ( ds.status() != QDataStream::Ok )
			return false;

		if ( !_addFileHeader( &fileHeader ) )
			return false;

		// check if file headers exceeded size of central directory
		if ( archiveFile_.pos() - endOfCentralDirectory.offsetOfCentralDirectory > endOfCentralDirectory.sizeOfTheCentralDirectory )
			return false;

		// abort loading if archive closes
		if ( isWorkerAborted_ )
			return false;
	}

#if 0
	// load digital signature
	DigitalSignatureStruct digitalSignature;
	ds >> digitalSignature;
	if ( ds.status() != QDataStream::Ok )
		return false;
#endif

	if ( archiveFile_.pos() - endOfCentralDirectory.offsetOfCentralDirectory != endOfCentralDirectory.sizeOfTheCentralDirectory )
		return false;

	return true;
}


/** \internal
 * Converts DOS date/time format to Qt.
 */
inline QDateTime from_dos_date_time( int date, int time )
{
	const int day    = (date & 0x001f) >> 0;
	const int month  = (date & 0x01e0) >> 5;
	const int year   = (date & 0xfe00) >> 9;

	const int second = (time & 0x001f) >> 0;
	const int minute = (time & 0x07e0) >> 5;
	const int hour   = (time & 0xf800) >> 11;

	static const QDateTime dosStartDate = QDateTime( QDate( 1980, 1, 1 ) );
	return dosStartDate
		.addYears( year ).addMonths( month ).addDays( day )
		.addSecs( hour*3600 + minute*60 + second );
}


/**
 * Adds founded file header from central directory.
 * This also constructs all directories above this entry.
 */
bool ArchivePrivate::_addFileHeader( const void * fileHeaderP )
{
	const FileHeaderStruct & fileHeader = *static_cast<const FileHeaderStruct*>( fileHeaderP );

	if ( fileHeader.fileName.isEmpty() )
		return false;

	// locate parent entry
	ArchiveEntry * parentEntry = 0;
	QString fileName;
	QStringList dirNames;

	int slash = fileHeader.fileName.indexOf( QLatin1Char( '/' ) );

	if ( slash == -1 )
	{
		// no slash found in file path, so this is a root entry
		parentEntry = rootEntry_;
		fileName = fileHeader.fileName;
	}
	else
	{
		if ( slash == 0 )
			return false;

		int previousSlash = -1;
		while ( slash < fileHeader.fileName.length() )
		{
			QString dirName = fileHeader.fileName.mid( previousSlash + 1, slash - previousSlash - 1 );
			dirNames << dirName;

			previousSlash = slash;
			slash = fileHeader.fileName.indexOf( QLatin1Char( '/' ), slash + 1 );
			if ( slash == -1 )
			{
				fileName = fileHeader.fileName.mid( previousSlash + 1 );
				break;
			}
		}
	}

	if ( !dirNames.isEmpty() )
	{
		// construct sequence of directories for added entry
		parentEntry = rootEntry_;
		for ( int i = 0; i < dirNames.count(); ++i )
		{
			const QString dirName = dirNames.at( i );

			ArchiveEntry *& dirEntry = parentEntry->entryForName[ dirName ];

			if ( dirEntry )
			{
				// ensure this is a directory
				if ( !dirEntry->info.isDir )
					return false;

				// mark as existed
				dirEntry->existedAfterUpdate = true;
			}
			else
			{
				// create parent entry
				dirEntry = new ArchiveEntry;
				dirEntry->parentEntry = parentEntry;
				dirEntry->info.isDir = true;
				dirEntry->info.fileName = dirName;

				for ( int j = 0; j <= i; ++j )
				{
					dirEntry->info.filePath += dirNames.at( j );
					if ( j < i )
						dirEntry->info.filePath += QLatin1Char( '/' );
				}

				parentEntry->entries << dirEntry;
				entryForFilePath_[ dirEntry->info.filePath ] = dirEntry;
			}

			parentEntry = dirEntry;
		}
	}

	if ( fileName.isEmpty() )
	{
		// this was a directory path without file name
		// do nothing
		return true;
	}

	// converted modification time from DOS format
	const QDateTime modTime = from_dos_date_time( fileHeader.modDate, fileHeader.modTime );

	ArchiveEntry *& entry = parentEntry->entryForName[ fileName ];

	const bool existedBefore = entry != 0;

	if ( existedBefore )
	{
		// already have entry with the same file path
		// check if this file was changed since last update

		entry->existedAfterUpdate = true;

		entry->changedAfterUpdate =
			entry->info.size != fileHeader.uncompressedSize ||
			entry->info.modTime != modTime ||
			entry->info.crc32 != fileHeader.crc32;
	}
	else
	{
		entry = new ArchiveEntry;
	}

	// fill entry->info structure from the given file header
	entry->info.fileName = fileName;
	entry->info.filePath = fileHeader.fileName;
	entry->info.localFileHeaderOffset = fileHeader.localHeaderOffset;
	entry->info.compressedSize = fileHeader.compressedSize;
	entry->info.size = fileHeader.uncompressedSize;
	entry->info.modTime = modTime;
	entry->info.crc32 = fileHeader.crc32;
	entry->info.canRead =
		fileHeader.compressionMethod == 0 || // 0 means file not compressed
		fileHeader.compressionMethod == 8;   // 8 means Deflate algorithm
	entry->info.isSequential = fileHeader.compressionMethod != 0;
	entry->info.isDir = false;

	if ( !existedBefore )
	{
		entry->parentEntry = parentEntry;
		parentEntry->entries << entry;
		parentEntry->entryForName[ entry->info.fileName ] = entry;
		entryForFilePath_[ entry->info.filePath ] = entry;
	}

	return true;
}


/**
 * Processes \a requests in the worker thread.
 */
void ArchivePrivate::_processFileRequests( QList<ArchiveFileRequest*> & requests )
{
	while ( true )
	{
		QReadLocker contentsLocker( &contentsMutex_ );

		if ( isWorkerAborted_ )
		{
			// archive is closing, unlink all the given requests and wake them up

			QWriteLocker linkedFileInstancesLocker( &linkedFileInstancesMutex_ );

			// release copied requests and unlink them
			while ( !requests.isEmpty() )
			{
				ArchiveFileRequest * request = requests.takeFirst();

				Q_ASSERT( request->file()->entry_ );
				Q_ASSERT( request->file()->entry_->fileInstances.contains( request->file()->fileInstance_ ) );

				_cleanupOpenedFile( request->file() );

				request->file()->entry_->fileInstances.removeOne( request->file()->fileInstance_ );
				request->file()->entry_ = 0;

				Q_ASSERT( linkedFileInstances_.contains( request->file()->fileInstance_ ) );
				linkedFileInstances_.removeOne( request->file()->fileInstance_ );

				QWriteLocker fileRequestLocker( &request->file()->requestMutex_ );
				request->file()->request_ = 0;
				request->file()->requestWaiter_.wakeOne();
			}
			return;
		}

		if ( requests.isEmpty() )
			break;

		ArchiveFileRequest * request = requests.takeFirst();

		bool done = false;

		switch ( request->type() )
		{
		case ArchiveFileRequest::Open:
			done = _processFileOpenRequest( static_cast<ArchiveFileOpenRequest*>( request ) );
			break;
		case ArchiveFileRequest::Close:
			done = _processFileCloseRequest( static_cast<ArchiveFileCloseRequest*>( request ) );
			break;
		case ArchiveFileRequest::Seek:
			done = _processFileSeekRequest( static_cast<ArchiveFileSeekRequest*>( request ) );
			break;
		case ArchiveFileRequest::Read:
			done = _processFileReadRequest( static_cast<ArchiveFileReadRequest*>( request ) );
			break;
		case ArchiveFileRequest::Write:
			done = _processFileWriteRequest( static_cast<ArchiveFileWriteRequest*>( request ) );
			break;
		case ArchiveFileRequest::Flush:
			done = _processFileFlushRequest( static_cast<ArchiveFileFlushRequest*>( request ) );
			break;
		}

		if ( done )
			request->setDone();

		QWriteLocker fileRequestLocker( &request->file()->requestMutex_ );
		request->file()->request_ = 0;
		request->file()->requestWaiter_.wakeOne();
	}
}


/**
 * Low-level inflate initialization of z-stream.
 */
inline bool ArchivePrivate::_openInflate( ArchiveFile * file )
{
	ArchiveEntry * entry = file->entry_;
	z_streamp zStream = &file->zStream_;

	// clean crc32
	file->zCrc32_ = 0;

	// fill stream fields
	zStream->zalloc = 0;
	zStream->zfree = 0;
	zStream->opaque = 0;
	zStream->next_in = 0;
	zStream->avail_in = 0;
	zStream->total_out = 0;

	const int error = inflateInit2( zStream, -MAX_WBITS );

	if ( error != Z_OK )
		return false;

	file->zCompressedPos_ = 0;
	file->zRestCompressed_ = entry->info.compressedSize;
	file->zRestUncompressed_ = entry->info.size;

	return true;
}


/**
 * Low-level inflate deinitialization of z-stream.
 */
inline void ArchivePrivate::_closeInflate( ArchiveFile * file )
{
	z_streamp zStream = &file->zStream_;

	// assuming file was opened before, otherwise we will not reach here
	inflateEnd( zStream );
	file->zReadBuffer_ = QByteArray();
}


/**
 * Looks up for the real start offset of \a entry inside archive and jumps to it.
 * Unfortunately this is not possible on central directory load, because each
 * file header contains variable file name and extra info we don't want to parse
 * instantly to speedup update process.
 * Also caches this start offset value inside \a entry.
 */
inline bool ArchivePrivate::_seekDataOffset( ArchiveEntry * entry )
{
	if ( entry->info.dataOffset == -1 )
	{
		if ( !archiveFile_.seek( entry->info.localFileHeaderOffset ) )
			return false;

		QDataStream ds( &archiveFile_ );
		ds.setByteOrder( QDataStream::LittleEndian );

		// dummy just skips data stream, without reading file name and extra info
		DummyLocalFileHeaderStruct dummy;
		ds >> dummy;

		if ( ds.status() != QDataStream::Ok )
			return false;

		entry->info.dataOffset = archiveFile_.pos();
	}
	else
	{
		if ( !archiveFile_.seek( entry->info.dataOffset ) )
			return false;
	}

	return true;
}


/**
 * Cleans up resources for earlier opened \a file.
 */
void ArchivePrivate::_cleanupOpenedFile( ArchiveFile * file )
{
	Q_ASSERT( file->entry_ );

	if ( !openedFileInstances_.contains( file->fileInstance_ ) )
		return;

	// file is opened, cleanup resources
	if ( file->entry_->info.isSequential )
	{
		_closeInflate( file );
	}
	else
	{
		// nothing to cleanup for random-access files
	}

	openedFileInstances_.removeOne( file->fileInstance_ );
}


bool ArchivePrivate::_processFileOpenRequest( ArchiveFileOpenRequest * openRequest )
{
	ArchiveFile * file = openRequest->file();
	ArchiveEntry * entry = file->entry_;

	if ( !_seekDataOffset( entry ) )
		return false;

	if ( !entry->info.isSequential )
	{
		// file is not compressed, do nothing
	}
	else
	{
		if ( !_openInflate( file ) )
			return false;
	}

	openedFileInstances_ << file->fileInstance_;

	return true;
}


bool ArchivePrivate::_processFileCloseRequest( ArchiveFileCloseRequest * closeRequest )
{
	ArchiveFile * file = closeRequest->file();
	ArchiveEntry * entry = file->entry_;

	if ( !entry->info.isSequential )
	{
		// file is not compressed, do nothing
	}
	else
	{
		_closeInflate( file );
	}

	openedFileInstances_.removeOne( file->fileInstance_ );

	return true;
}


bool ArchivePrivate::_processFileSeekRequest( ArchiveFileSeekRequest * seekRequest )
{
	ArchiveFile * file = seekRequest->file();
	ArchiveEntry * entry = file->entry_;

	if ( !entry->info.isSequential )
	{
		// ensure seek pos is in range of uncompressed data
		if ( seekRequest->pos() < 0 || seekRequest->pos() > entry->info.size )
			return false;
	}
	else
	{
		// for sequential devices only seeking to 0 offset is allowed
		if ( seekRequest->pos() != 0 )
			return false;

		_closeInflate( file );
		const bool isReopened = _openInflate( file );
		Q_UNUSED( isReopened );
		Q_ASSERT( isReopened );
	}

	return true;
}


bool ArchivePrivate::_processFileReadRequest( ArchiveFileReadRequest * readRequest )
{
	ArchiveFile * file = readRequest->file();
	ArchiveEntry * entry = file->entry_;
	z_streamp zStream = &file->zStream_;

	if ( !entry->info.isSequential )
	{
		// file is not compressed
		if ( !archiveFile_.seek( entry->info.dataOffset + file->pos_ ) )
			return false;

		const qint64 bytesToRead = qMin<qint64>( readRequest->maxlen(), entry->info.size - file->pos_ );
		const qint64 bytes = archiveFile_.read( readRequest->data(), bytesToRead );

		readRequest->setResult( bytes );

		return true;
	}

	zStream->next_out = (Bytef*)readRequest->data();
	zStream->avail_out = qMin<qint64>( readRequest->maxlen(), file->zRestUncompressed_ );

	// init buffer for reading compressed data
	{
		static const int BufferSize = 16384;
		if ( file->zReadBuffer_.isNull() )
			file->zReadBuffer_.resize( BufferSize );
	}

	qint64 totalUncompressedBytes = 0;

	while ( zStream->avail_out > 0 )
	{
		if ( zStream->avail_in == 0 && file->zRestCompressed_ > 0 )
		{
			const qint64 compressedBytes = qMin<qint64>( file->zRestCompressed_, file->zReadBuffer_.size() );

			archiveFile_.seek( entry->info.dataOffset + file->zCompressedPos_ );

			if ( archiveFile_.read( file->zReadBuffer_.data(), compressedBytes ) != compressedBytes )
			{
				// should not happen, because we know exact size of compressed data
				return false;
			}

			file->zCompressedPos_ += compressedBytes;
			file->zRestCompressed_ -= compressedBytes;
			zStream->next_in = (Bytef*)file->zReadBuffer_.constData();
			zStream->avail_in = (uInt)compressedBytes;
		}

		const qint64 totalOutBefore = zStream->total_out;
		const char * outBufferBefore = (char*)zStream->next_out;

		int error = inflate( zStream, Z_SYNC_FLUSH );

		if ( error != Z_OK && error != Z_STREAM_END && error != Z_BUF_ERROR )
		{
#ifdef GRIM_ARCHIVE_DEBUG
			qDebug() << "ArchivePrivate::_processFileReadRequest() : Error reading compressed data";
#endif
			return false;
		}

		const qint64 totalOutAfter = zStream->total_out;
		const qint64 uncompressedBytes = totalOutAfter - totalOutBefore;

		file->zCrc32_ = crc32( file->zCrc32_, (const Bytef*)outBufferBefore, uncompressedBytes );
		totalUncompressedBytes += uncompressedBytes;
		file->zRestUncompressed_ -= uncompressedBytes;

		if ( error == Z_STREAM_END )
		{
			// compressed stream finished
			if ( file->zRestCompressed_ == 0 )
			{
				if ( file->zRestUncompressed_ != 0 )
					qWarning( "Grim::ArchivePrivate::_processFileReadRequest() : Uncompressed size not matched." );
				if ( file->zCrc32_ != entry->info.crc32 )
					qWarning( "Grim::ArchivePrivate::_processFileReadRequest() : CRC32 not matched." );
			}
			break;
		}
	}

	readRequest->setResult( totalUncompressedBytes );

	return true;
}


bool ArchivePrivate::_processFileWriteRequest( ArchiveFileWriteRequest * writeRequest )
{
	return true;
}


bool ArchivePrivate::_processFileFlushRequest( ArchiveFileFlushRequest * flushRequest )
{
	return true;
}




} // namespace Grim
