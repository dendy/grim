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

#include "archive.h"

#include <QAbstractFileEngine>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QThreadStorage>
#include <QHash>
#include <QSharedDataPointer>
#include <QEvent>
#include <QBasicTimer>

#include <zlib.h>




namespace Grim {




extern QString toShortPath( const QString & path );
extern QString toLongPath( const QString & path );
extern QString softToHardCleanPath( const QString & path );
extern QString toSoftCleanPath( const QString & path );




class ArchiveFile;
class ArchivePrivate;
class ArchiveWorker;




class ArchiveFileRequest
{
public:
	enum Type
	{
		Null = 0,
		Open,
		Close,
		Seek,
		Flush,
		Read,
		Write
	};

	inline ArchiveFileRequest( ArchiveFile * file, Type type ) :
		file_( file ), type_( type ), isDone_( false )
	{}

	inline ArchiveFile * file() const
	{ return file_; }

	inline Type type() const
	{ return type_; }

	inline bool isDone() const
	{ return isDone_; }

	inline void setDone()
	{ isDone_ = true; }

private:
	ArchiveFile * file_;
	Type type_;
	bool isDone_;
};




class ArchiveFileOpenRequest : public ArchiveFileRequest
{
public:
	inline ArchiveFileOpenRequest( ArchiveFile * file, QIODevice::OpenMode mode ) :
		ArchiveFileRequest( file, ArchiveFileRequest::Open ), mode_( mode )
	{}

	inline QIODevice::OpenMode mode() const
	{ return mode_; }

private:
	QIODevice::OpenMode mode_;
};




class ArchiveFileCloseRequest : public ArchiveFileRequest
{
public:
	inline ArchiveFileCloseRequest( ArchiveFile * file ) :
		ArchiveFileRequest( file, ArchiveFileRequest::Close )
	{}
};




class ArchiveFileSeekRequest : public ArchiveFileRequest
{
public:
	inline ArchiveFileSeekRequest( ArchiveFile * file, qint64 pos ) :
		ArchiveFileRequest( file, ArchiveFileRequest::Seek ), pos_( pos )
	{}

	inline qint64 pos() const
	{ return pos_; }

private:
	qint64 pos_;
};




class ArchiveFileReadRequest : public ArchiveFileRequest
{
public:
	inline ArchiveFileReadRequest( ArchiveFile * file, char * data, qint64 maxlen ) :
		ArchiveFileRequest( file, ArchiveFileRequest::Read ), data_( data ), maxlen_( maxlen )
	{}

	inline char * data() const
	{ return data_; }

	inline qint64 maxlen() const
	{ return maxlen_; }

	inline qint64 result() const
	{ return result_; }

	inline void setResult( qint64 result )
	{ result_ = result; }

private:
	char * data_;
	qint64 maxlen_;
	qint64 result_;
};




class ArchiveFileWriteRequest : public ArchiveFileRequest
{
public:
	inline ArchiveFileWriteRequest( ArchiveFile * file, const char * data, qint64 len ) :
		ArchiveFileRequest( file, ArchiveFileRequest::Write ), data_( data ), len_( len )
	{}

	inline const char * data() const
	{ return data_; }

	inline qint64 len() const
	{ return len_; }

	inline qint64 result() const
	{ return result_; }

	inline void setResult( qint64 result )
	{ result_ = result; }

private:
	const char * data_;
	qint64 len_;
	qint64 result_;
};




class ArchiveFileFlushRequest : public ArchiveFileRequest
{
public:
	inline ArchiveFileFlushRequest( ArchiveFile * file ) :
		ArchiveFileRequest( file, ArchiveFileRequest::Flush )
	{}
};




class ArchiveInstanceData : public QSharedData
{
public:
	inline ArchiveInstanceData( ArchivePrivate * _archive ) :
		archive( _archive )
	{}

	inline ~ArchiveInstanceData()
	{}

	QReadWriteLock mutex;
	ArchivePrivate * archive;
};




class ArchiveInstance
{
public:
	ArchiveInstance();

	inline ArchiveInstance( ArchiveInstanceData * _d ) :
		d( _d )
	{}

	bool isNull() const;

	inline bool operator==( const ArchiveInstance & archiveInstance ) const
	{ return d == archiveInstance.d; }

	QExplicitlySharedDataPointer<ArchiveInstanceData> d;
};




class ArchiveInstanceLocker
{
public:
	inline ArchiveInstanceLocker( const ArchiveInstance & archiveInstance ) :
		copy_( archiveInstance )
	{
		copy_.d->mutex.lockForRead();
	}

	inline ~ArchiveInstanceLocker()
	{
		copy_.d->mutex.unlock();
	}

	inline ArchivePrivate * archive() const
	{
		return copy_.d->archive;
	}

private:
	ArchiveInstance copy_;
};




class ArchiveFileInstanceData : public QSharedData
{
public:
	ArchiveFileInstanceData( ArchiveFile * _file ) :
		file( _file )
	{}

	QReadWriteLock mutex;
	ArchiveFile * file;
};




class ArchiveFileInstance
{
public:
	inline ArchiveFileInstance( ArchiveFileInstanceData * _d ) :
		d( _d )
	{}

	inline bool operator==( const ArchiveFileInstance & fileInstance ) const
	{ return d == fileInstance.d; }

	QExplicitlySharedDataPointer<ArchiveFileInstanceData> d;
};




class ArchiveThreadCache
{
public:
	inline ArchiveThreadCache() :
		isManagerDisabled( false )
	{}

	bool isManagerDisabled;
	QList<ArchiveInstance> disabledArchives;
};


extern ArchiveThreadCache * archiveThreadCache();




class ArchiveEntryInfo
{
public:
	ArchiveEntryInfo();

	QString filePath;             // path to file relative to archive root
	QString fileName;             // file name only, excluding parent directories
	qint64 localFileHeaderOffset; // local file header offset
	qint64 dataOffset;            // local file data offset in zip-archive
	qint64 compressedSize;        // compressed file size
	qint64 size;                  // file size
	QDateTime modTime;            // last modification time
	quint32 crc32;                // crc32
	bool canRead;                 // only deflate supported
	bool isSequential;            // is sequential, i.e. compressed
	bool isDir;                   // entry is a directory
};




class ArchiveEntry
{
public:
	inline ArchiveEntry() :
		parentEntry( 0 ),
		existedBeforeUpdate( false ),
		existedAfterUpdate( true ),
		changedAfterUpdate( false )
	{}

	ArchiveEntry * parentEntry;
	QList<ArchiveEntry*> entries;
	QHash<QString,ArchiveEntry*> entryForName;

	ArchiveEntryInfo info;

	QList<ArchiveFileInstance> fileInstances;

	bool existedBeforeUpdate;
	bool existedAfterUpdate;
	bool changedAfterUpdate;
};




class ArchivePrivate : public QObject
{
	Q_OBJECT

public:
	enum EventType
	{
		EventType_StateChanged = QEvent::User + 1
	};

	class StateChangedEvent : public QEvent
	{
	public:
		StateChangedEvent( int s, bool b ) :
			QEvent( (QEvent::Type)EventType_StateChanged ),
			state( s ), isBroken( b )
		{}

		int state;
		bool isBroken;
	};

	ArchivePrivate( Archive * archive );
	~ArchivePrivate();

	void setFileName( const QString & fileName );
	void setMountPoint( const QString & mountPoint );

	QString actualMountPoint() const;
	QString cleanMountPointPath() const;

	QReadWriteLock * initializationMutex() const;

	bool open( Grim::Archive::OpenMode openMode );
	void close();

	bool isInitialized() const;

	Grim::Archive::OpenMode openMode() const;

	bool treatAsDir() const;
	void setTreatAsDir( bool set );

	QString globalComment() const;

	void registerFile( ArchiveFile * file );
	void unregisterFile( ArchiveFile * file );

	void linkFile( ArchiveFile * file );
	void unlinkFile( ArchiveFile * file );

	ArchiveEntry * entryForFilePath( const QString & filePath ) const;

	QReadWriteLock * contentsMutex() const;

//	QFileInfo fileInfoForEntry( ArchiveEntry * entry );

	void processFileRequest( ArchiveFileRequest * request );

protected:
	bool event( QEvent * e );
	void timerEvent( QTimerEvent * e );

private:
	void _setState( Archive::State state, bool isBroken );

	void _makeCleanMountPointPath();

	void _setTemporaryDisabled( bool set );

	bool _openArchive( Grim::Archive::OpenMode openMode );

	void _abortWorker();
	void _workerBody();

	bool _updateArchive();
	bool _loadCentralDirectory();
	bool _addFileHeader( const void * fileHeaderP );

	bool _openInflate( ArchiveFile * file );
	void _closeInflate( ArchiveFile * file );
	bool _seekDataOffset( ArchiveEntry * entry );
	void _cleanupOpenedFile( ArchiveFile * file );

	void _processFileRequests( QList<ArchiveFileRequest*> & requests );
	bool _processFileOpenRequest( ArchiveFileOpenRequest * openRequest );
	bool _processFileCloseRequest( ArchiveFileCloseRequest * closeRequest );
	bool _processFileSeekRequest( ArchiveFileSeekRequest * seekRequest );
	bool _processFileReadRequest( ArchiveFileReadRequest * readRequest );
	bool _processFileWriteRequest( ArchiveFileWriteRequest * writeRequest );
	bool _processFileFlushRequest( ArchiveFileFlushRequest * flushRequest );

private:
	ArchiveInstance archiveInstance_;

	// global
	Archive * archive_;
	QString fileName_;
	QString fileNameAbsolutePath_;
	QString mountPoint_;
	QString mountPointAbsolutePath_;
	QString cleanMountPointPath_;
	int updateInterval_;

	Archive::State state_;
	bool isBroken_;
	Grim::Archive::OpenMode openMode_;
	bool treatAsDir_;

	QReadWriteLock initializationMutex_;
	bool isInitialized_;

	ArchiveWorker * worker_;

	QReadWriteLock invokationMutex_;
	QWaitCondition invokationWaiter_;
	bool isWorkerAborted_;
	bool isInvoked_;

	QFile archiveFile_;

	// blocker waiter
	QMutex blockMutex_;
	QWaitCondition blockWaiter_;

	// job
	QReadWriteLock jobMutex_;
	QWaitCondition jobWaiter_;
	bool isWaitingForJob_;

	// file requests
	QList<ArchiveFileRequest*> requests_;
	bool isTimeToUpdate_;

	// update
	bool wasInitialUpdate_;
	QDateTime archiveLastModified_;
	bool isArchiveDirty_;
	QTime updateIntervalTime_;
	QBasicTimer updateTimer_;

	// contents
	QReadWriteLock contentsMutex_;

	QString globalComment_;
	QHash<QString,ArchiveEntry*> entryForFilePath_;
	ArchiveEntry * rootEntry_;

	// registered files
	QReadWriteLock fileInstancesMutex_;
	QList<ArchiveFileInstance> fileInstances_;

	// linked files
	QReadWriteLock linkedFileInstancesMutex_;
	QList<ArchiveFileInstance> linkedFileInstances_;

	// opened files
	QList<ArchiveFileInstance> openedFileInstances_;

	friend class ArchiveWorker;
	friend class Archive;
};




class ArchiveFile : public QAbstractFileEngine
{
public:
	ArchiveFile( const ArchiveInstance & archiveInstance, const QString & fileName, const QString & absoluteFilePath,
		const QString & internalFileName, bool isRelativePath );
	~ArchiveFile();

	bool caseSensitive() const;
	bool isSequential() const;
	bool isRelativePath() const;

	void setFileName( const QString & fileName );

	QString fileName( FileName file ) const;
	FileFlags fileFlags( FileFlags type ) const;
	QDateTime fileTime( FileTime time ) const;

	QStringList entryList( QDir::Filters filters, const QStringList & filterNames ) const;
	QAbstractFileEngine::Iterator * beginEntryList( QDir::Filters filters, const QStringList & filterNames );

	bool open( QIODevice::OpenMode mode );
	bool close();

	qint64 pos() const;
	bool seek( qint64 pos );
	qint64 read( char * data, qint64 maxlen );
	qint64 write( const char * data, qint64 len );
	bool flush();

	qint64 size() const;
	bool setSize( qint64 size );

	bool setPermissions( uint perms );

	bool mkdir( const QString & dirName, bool createParentDirectories ) const;

	QString owner( FileOwner owner ) const;
	uint ownerId( FileOwner owner ) const;

	bool supportsExtension( Extension extension ) const;
	bool extension( Extension extension, const ExtensionOption * option, ExtensionReturn * output );

private:
	void _updateFileNames();

private:
	ArchiveFileInstance fileInstance_;
	ArchiveInstance archiveInstance_;

	QString fileName_;
	QString internalFileName_;

	// file names for QAbstractFileEngine::FileName enum
	QString fileNameBase_;
	QString fileNamePath_;
	QString fileNameAbsolute_;
	QString fileNameAbsolutePath_;

	bool isRelativePath_;

	// mutable only from file
	QIODevice::OpenMode openMode_;
	qint64 pos_;

	// linked entry
	ArchiveEntry * entry_;

	// requests
	QWaitCondition requestWaiter_;
	QReadWriteLock requestMutex_;
	ArchiveFileRequest * request_;

	// mutable only from archive worker
	quint32 zCrc32_;
	z_stream zStream_;
	QByteArray zReadBuffer_;
	qint64 zCompressedPos_;
	qint64 zRestCompressed_;
	qint64 zRestUncompressed_;

	friend class ArchiveManagerPrivate;
	friend class ArchivePrivate;
	friend class ArchiveFileIterator;
};




class ArchiveFileIterator : public QAbstractFileEngineIterator
{
public:
	ArchiveFileIterator( ArchiveFile * file, QDir::Filters filters, const QStringList & filterNames );
	~ArchiveFileIterator();

	QString currentFileName() const;

	bool hasNext() const;
	QString next();

private:
	void _update();

private:
	ArchiveFile * file_;
	int index_;
	bool isValid_;
	bool isDirty_;
	QStringList fileNames_;
};




inline ArchiveEntryInfo::ArchiveEntryInfo() :
	localFileHeaderOffset( -1 ),
	dataOffset( -1 ),
	compressedSize( 0 ),
	size( 0 ),
	crc32( 0 ),
	canRead( true ),
	isSequential( false ),
	isDir( true )
{}




inline bool ArchivePrivate::isInitialized() const
{ return isInitialized_; }

inline QString ArchivePrivate::actualMountPoint() const
{ return !mountPoint_.isNull() ? mountPointAbsolutePath_ : fileNameAbsolutePath_; }

inline QString ArchivePrivate::cleanMountPointPath() const
{ return cleanMountPointPath_; }

inline QReadWriteLock * ArchivePrivate::initializationMutex() const
{ return const_cast<QReadWriteLock*>( &initializationMutex_ ); }

inline QReadWriteLock * ArchivePrivate::contentsMutex() const
{ return const_cast<QReadWriteLock*>( &contentsMutex_ ); }




inline bool ArchiveFile::caseSensitive() const
{ return true; }




} // namespace Grim
