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

#include <QRegExp>
#include <QDebug>




namespace Grim {




static const QString DotFileName    = QLatin1String( "." );
static const QString DotDotFileName = QLatin1String( ".." );




/** \internal
 *
 * \class Filter
 *
 * Helper class that matches the given file/dir path for the given filters and name wildcards.
 */

class Filter
{
public:
	Filter( QDir::Filters filters, const QStringList & names ) :
		filters_( filters )
	{
		isAll_ = false;

		for ( QStringListIterator it( names ); it.hasNext(); )
		{
			const QString name = it.next();
			if ( name == QLatin1String( "*" ) )
			{
				isAll_ = true;
				regExps_.clear();
				break;
			}

			regExps_ << QRegExp( name, Qt::CaseSensitive, QRegExp::Wildcard );
		}
	}

	inline bool testDir( const QString & fileName ) const
	{
		if ( filters_ & QDir::AllDirs )
			return true;

		if ( !(filters_ & QDir::Dirs) )
			return false;

		if ( isAll_ )
			return true;

		for ( QListIterator<QRegExp> it( regExps_ ); it.hasNext(); )
			if ( fileName.count( it.next() ) > 0 )
				return true;

		return false;
	}

	inline bool testFile( const QString & fileName ) const
	{
		if ( !(filters_ & QDir::Files) )
			return false;

		if ( isAll_ )
			return true;

		for ( QListIterator<QRegExp> it( regExps_ ); it.hasNext(); )
			if ( fileName.count( it.next() ) > 0 )
				return true;

		return false;
	}

private:
	bool isAll_;
	QDir::Filters filters_;
	QList<QRegExp> regExps_;
};




/** \internal
 *
 * \class ArchiveFile
 *
 * Implementation of QAbstractFileEngine.
 * Represents single file instance inside concrete archive.
 * Lives in random client thread and links with archive instance on demand.
 *
 * According to Qt this class is reentrant.
 */

ArchiveFile::ArchiveFile( const ArchiveInstance & archiveInstance,
	const QString & fileName, const QString & absoluteFilePath,
	const QString & internalFileName, bool isRelativePath ) :
	fileInstance_( new ArchiveFileInstanceData( this ) ),
	archiveInstance_( archiveInstance ),
	fileName_( fileName ),
	internalFileName_( internalFileName ),
	fileNameAbsolute_( absoluteFilePath ),
	isRelativePath_( isRelativePath ),
	pos_( -1 ),
	entry_( 0 ),
	request_( 0 )
{
}


ArchiveFile::~ArchiveFile()
{
	Q_ASSERT_X( openMode_ == QIODevice::NotOpen && pos_ == -1,
		"Grim::File::~File()",
		"File must be closed." );

	{
		ArchiveInstanceLocker archiveLocker( archiveInstance_ );

		if ( archiveLocker.archive() )
		{
			{
				// we are locking for write because of recursive archive contents mutex,
				// which can be also in write mode inside ArchivePrivate::close()
				QWriteLocker contentsLocker( archiveLocker.archive()->contentsMutex() );
				archiveLocker.archive()->unlinkFile( this );
			}

			archiveLocker.archive()->unregisterFile( this );
		}
	}

	QWriteLocker selfLocker( &fileInstance_.d->mutex );
	fileInstance_.d->file = 0;
}


bool ArchiveFile::isSequential() const
{
	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return false;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	archiveLocker.archive()->linkFile( const_cast<ArchiveFile*>( this ) );

	if ( !entry_ )
		return false;

	return entry_->info.isSequential;
}


void ArchiveFile::setFileName( const QString & file_name )
{
	// just ignore?
}


bool ArchiveFile::isRelativePath() const
{
	return isRelativePath_;
}


void ArchiveFile::_updateFileNames()
{
	if ( !fileNameBase_.isEmpty() )
		return;

	// obtain names for BaseName and PathName
	{
		const int length = fileName_.length();

		int veryLastSlashPos = length;
		while ( veryLastSlashPos > 0 && fileName_.at( veryLastSlashPos - 1 ) == QLatin1Char( '/' ) )
			veryLastSlashPos--;

		const int lastSlashPos = fileName_.lastIndexOf( QLatin1Char( '/' ), veryLastSlashPos - length - 2 );
		fileNameBase_ = fileName_.mid( lastSlashPos + 1, veryLastSlashPos - lastSlashPos - 1 );
		fileNamePath_ = lastSlashPos == -1 ? QString() : fileName_.left( lastSlashPos );
	}

	// obtain name for AbsolutePathName
	{
		const int lastSlashPos = fileNameAbsolute_.lastIndexOf( QLatin1Char( '/' ), -2 );
		fileNameAbsolutePath_ = fileNameAbsolute_.left( lastSlashPos );
	}

#if 0
#ifdef GRIM_ARCHIVE_DEBUG
	qDebug() << "ArchiveFile::_updateFileNames() :" << this;
	qDebug() << "ArchiveFile::_updateFileNames() : fileName_ =" << fileName_;
	qDebug() << "ArchiveFile::_updateFileNames() : fileNameBase_ =" << fileNameBase_;
	qDebug() << "ArchiveFile::_updateFileNames() : fileNamePath_ =" << fileNamePath_;
	qDebug() << "ArchiveFile::_updateFileNames() : fileNameAbsolute_ =" << fileNameAbsolute_;
	qDebug() << "ArchiveFile::_updateFileNames() : fileNameAbsolutePath_ =" << fileNameAbsolutePath_;
#endif
#endif
}


QString ArchiveFile::fileName( FileName file ) const
{
	switch ( file )
	{
	case QAbstractFileEngine::DefaultName:
		return fileName_;

	case QAbstractFileEngine::BaseName:
		const_cast<ArchiveFile*>( this )->_updateFileNames();
		return fileNameBase_;

	case QAbstractFileEngine::PathName:
		const_cast<ArchiveFile*>( this )->_updateFileNames();
		return fileNamePath_;

	case QAbstractFileEngine::AbsoluteName:
		const_cast<ArchiveFile*>( this )->_updateFileNames();
		return fileNameAbsolute_;

	case QAbstractFileEngine::AbsolutePathName:
		const_cast<ArchiveFile*>( this )->_updateFileNames();
		return fileNameAbsolutePath_;

	case QAbstractFileEngine::LinkName:
	case QAbstractFileEngine::CanonicalName:
	case QAbstractFileEngine::CanonicalPathName:
	case QAbstractFileEngine::BundleName:
		return fileName_;
	}

	return fileName_;
}


QAbstractFileEngine::FileFlags ArchiveFile::fileFlags( FileFlags type ) const
{
	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return 0;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	archiveLocker.archive()->linkFile( const_cast<ArchiveFile*>( this ) );

	if ( !entry_ )
		return 0;

	FileFlags flags =
		QAbstractFileEngine::ReadOwnerPerm |
		QAbstractFileEngine::ReadUserPerm |
		QAbstractFileEngine::ReadGroupPerm |
		QAbstractFileEngine::ReadOtherPerm |
		QAbstractFileEngine::ExistsFlag;

	if ( entry_->info.isDir )
	{
		flags |= QAbstractFileEngine::DirectoryType;
	}
	else
	{
		flags |= QAbstractFileEngine::FileType;
	}

	return flags & type;
}


QDateTime ArchiveFile::fileTime( FileTime time ) const
{
	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return QDateTime();

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	archiveLocker.archive()->linkFile( const_cast<ArchiveFile*>( this ) );

	if ( !entry_ )
		return QDateTime();

	switch ( time )
	{
	case ModificationTime:
		return entry_->info.modTime;
	}

	return QDateTime();
}


QStringList ArchiveFile::entryList( QDir::Filters filters, const QStringList & filterNames ) const
{
	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return QStringList();

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	archiveLocker.archive()->linkFile( const_cast<ArchiveFile*>( this ) );

	if ( !entry_ )
		return QStringList();

	if ( !entry_->info.isDir )
		return QStringList();

	Filter filter( filters, filterNames );

	QStringList list;

	if ( !(filters & QDir::NoDotAndDotDot) )
		list << DotFileName << DotDotFileName;

	QList<ArchiveEntry*>::Iterator end = entry_->entries.end();
	for ( QList<ArchiveEntry*>::Iterator it = entry_->entries.begin(); it != end; ++it )
	{
		const ArchiveEntry * entry = *it;

		if ( entry->info.isDir )
		{
			if ( filter.testDir( entry->info.fileName ) )
				list << entry->info.fileName;
		}
		else
		{
			if ( filter.testFile( entry->info.fileName ) )
				list << entry->info.fileName;
		}
	}

	return list;
}


QAbstractFileEngineIterator * ArchiveFile::beginEntryList( QDir::Filters filters, const QStringList & filterNames )
{
	return new ArchiveFileIterator( this, filters, filterNames );
}


bool ArchiveFile::open( QIODevice::OpenMode mode )
{
	if ( mode & QIODevice::WriteOnly )
	{
		qWarning( "Grim::ArchiveFile::open() : Only Read-only opening mode currently possible." );
		return false;
	}

	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return false;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	archiveLocker.archive()->linkFile( this );

	// no such file inside archive or file is unreadable for us
	if ( !entry_ )
		return false;

	ArchiveFileOpenRequest openRequest( this, mode );
	archiveLocker.archive()->processFileRequest( &openRequest );

	if ( !openRequest.isDone() )
		return false;

	openMode_ = mode;
	pos_ = 0;

	return true;
}


bool ArchiveFile::close()
{
	if ( openMode_ == QIODevice::NotOpen )
		return true;

	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	// mark as closed anyway
	openMode_ = QIODevice::NotOpen;
	pos_ = -1;

	if ( !archiveLocker.archive() )
		return false;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	if ( !entry_ )
		return false;

	ArchiveFileCloseRequest closeRequest( this );
	archiveLocker.archive()->processFileRequest( &closeRequest );

	if ( !closeRequest.isDone() )
		return false;

	return true;
}


qint64 ArchiveFile::pos() const
{
	return pos_;
}


bool ArchiveFile::seek( qint64 pos )
{
	// check if file is opened
	if ( pos_ == -1 )
	{
		qWarning( "Grim::ArchiveFile::seek() : File is not opened." );
		return false;
	}

	if ( pos < 0 )
		return false;

	if ( pos == pos_ )
		return true;

	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return false;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	if ( !entry_ )
		return false;

	if ( pos > entry_->info.size )
		return false;

	if ( entry_->info.isSequential )
	{
		// Two policies at this stage for sequential devices.
#if 0
		// Policy 1: Completely deny seeking.
		return false;
#endif
#if 1
		// Policy 2: Allow rewinding to the zero position only.
		if ( pos != 0 )
			return false;
#endif
	}

	ArchiveFileSeekRequest seekRequest( this, pos );
	archiveLocker.archive()->processFileRequest( &seekRequest );

	if ( !seekRequest.isDone() )
		return false;

	pos_ = pos;

	return true;
}


qint64 ArchiveFile::read( char * data, qint64 maxlen )
{
	// check is file is opened
	if ( pos_ == -1 )
	{
		qWarning( "Grim::ArchiveFile::read() : File is not opened." );
		return -1;
	}

	if ( !(openMode_ & QIODevice::ReadOnly) )
	{
		qWarning( "Grim::ArchiveFile::read() : File is not opened for reading." );
		return -1;
	}

	if ( maxlen < 0 )
		return -1;

	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return -1;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	if ( !entry_ )
		return -1;

	if ( pos_ == entry_->info.size )
	{
		// already at end
		return 0;
	}

	ArchiveFileReadRequest readRequest( this, data, maxlen );
	archiveLocker.archive()->processFileRequest( &readRequest );

	if ( !readRequest.isDone() )
		return -1;

	pos_ += readRequest.result();

	return readRequest.result();
}


qint64 ArchiveFile::write( const char * data, qint64 len )
{
	return -1;
}


bool ArchiveFile::flush()
{
	return false;
}


qint64 ArchiveFile::size() const
{
	ArchiveInstanceLocker archiveLocker( archiveInstance_ );

	if ( !archiveLocker.archive() )
		return -1;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	archiveLocker.archive()->linkFile( const_cast<ArchiveFile*>( this ) );

	if ( !entry_ )
		return -1;

	return entry_->info.size;
}


bool ArchiveFile::setSize( qint64 size )
{
	return false;
}


bool ArchiveFile::setPermissions( uint /*perms*/ )
{
	// TODO: Add setting permissions
	return false;
}


bool ArchiveFile::mkdir( const QString & dirName, bool createParentDirectories ) const
{
	return false;
}


QString ArchiveFile::owner( FileOwner owner ) const
{
	return QString();
}


uint ArchiveFile::ownerId( FileOwner owner ) const
{
	return 0;
}


bool ArchiveFile::supportsExtension( Extension extension ) const
{
	return extension == AtEndExtension;
}


bool ArchiveFile::extension( Extension extension, const ExtensionOption * option, ExtensionReturn * output )
{
	switch ( extension )
	{
	case AtEndExtension:
	{
		if ( pos_ == -1 )
			return false;

		ArchiveInstanceLocker archiveLocker( archiveInstance_ );

		if ( !archiveLocker.archive() )
			return false;

		QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

		if ( !entry_ )
			return false;

		return pos_ == entry_->info.size;
	}

	default:
		return false;
	}
}




/** \internal
 *
 * \class ArchiveFileIterator
 *
 * Implementation of QAbstractFileEngineIterator for the ArchiveFile.
 */

ArchiveFileIterator::ArchiveFileIterator( ArchiveFile * file, QDir::Filters filters, const QStringList & filterNames ) :
	QAbstractFileEngineIterator( filters, filterNames ),
	file_( file ), index_( -1 ), isDirty_( true )
{
}


ArchiveFileIterator::~ArchiveFileIterator()
{
}


/**
 * Links to the archive and collects list of file names inside entry to which path() is points.
 */
inline void ArchiveFileIterator::_update()
{
	if ( !isDirty_ )
		return;

	isDirty_ = false;
	isValid_ = false;

	ArchiveInstanceLocker archiveLocker( file_->archiveInstance_ );

	if ( !archiveLocker.archive() )
		return;

	QReadLocker contentsLocker( archiveLocker.archive()->contentsMutex() );

	ArchiveEntry * entry = 0;

	if ( path() == file_->fileName_ )
	{
		// path() is the same as for the parent file
		// link it, this can be quicker than locating entry from scratch,
		// because parent file can be already linked as well
		archiveLocker.archive()->linkFile( file_ );
		if ( !file_->entry_ )
			return;
		entry = file_->entry_;
	}
	else
	{
		// path() is not the same as for the parent file
		// this means that iterator searched thru the subdirectories
		// of parent file
		entry = archiveLocker.archive()->entryForFilePath( path() );
	}

	if ( entry )
	{
		if ( !entry->info.isDir )
			return;

		Filter filter( filters(), nameFilters() );

		for ( QListIterator<ArchiveEntry*> it( entry->entries ); it.hasNext(); )
		{
			ArchiveEntry * childEntry = it.next();

			if ( childEntry->info.isDir )
			{
				if ( filter.testDir( childEntry->info.fileName ) )
					fileNames_ << childEntry->info.fileName;
			}
			else
			{
				if ( filter.testFile( childEntry->info.fileName ) )
					fileNames_ << childEntry->info.fileName;
			}
		}
	}
	else
	{
		// entry not found
		//fileNames_ = QDir( path() ).entryList( nameFilters(), filters() );
	}

	isValid_ = true;
}


QString ArchiveFileIterator::currentFileName() const
{
	((ArchiveFileIterator*)this)->_update();

	if ( !isValid_ || index_ == -1 )
		return QString();

	return fileNames_.at( index_ );
}


bool ArchiveFileIterator::hasNext() const
{
	((ArchiveFileIterator*)this)->_update();

	if ( !isValid_ )
		return false;

	return index_ < fileNames_.count() - 1;
}


QString ArchiveFileIterator::next()
{
	_update();

	if ( !isValid_ || index_ == /*fileInfos_*/fileNames_.count() - 1 )
		return QString();

	index_++;
	QString filePath = currentFilePath();

	return filePath;
}




} // namespace Grim
