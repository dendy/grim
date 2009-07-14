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

#include "archivemanager.h"
#include "archivemanager_p.h"
#include "archive_p.h"

#include <QCoreApplication>
#include <QDebug>




namespace Grim {




static QReadWriteLock _sharedManagerMutex;
static ArchiveManager * _sharedManager = 0;




QAbstractFileEngine * ArchiveFileEngineHandler::create( const QString & fileName ) const
{
	return ArchiveManagerPrivate::sharedManagerPrivate()->createFileEngine( fileName );
}




void ArchiveManagerPrivate::destroySharedManager()
{
	QWriteLocker locker( &_sharedManagerMutex );
	delete _sharedManager;
	_sharedManager = 0;
}




ArchiveManagerPrivate * ArchiveManagerPrivate::sharedManagerPrivate()
{
	return ArchiveManager::sharedManager()->d_;
}


ArchiveManagerPrivate::ArchiveManagerPrivate()
{
	sharedNullArchiveInstanceData_ = 0;

	fileEngineHandler_ = new ArchiveFileEngineHandler;

	isEnabled_ = true;
}


ArchiveManagerPrivate::~ArchiveManagerPrivate()
{
	Q_ASSERT_X( registeredArchives_.isEmpty(),
		"Grim::ArchiveManager::~ArchiveManager()",
		"Archive instances must be closed explicitly." );

	delete fileEngineHandler_;

	if ( sharedNullArchiveInstanceData_ )
	{
		delete sharedNullArchiveInstanceData_;
		sharedNullArchiveInstanceData_ = 0;
	}
}


bool ArchiveManagerPrivate::isEnabled() const
{
	QReadLocker isEnabledLocker( const_cast<QReadWriteLock*>( &isEnabledMutex_ ) );
	return isEnabled_;
}


void ArchiveManagerPrivate::setEnabled( bool set )
{
	QWriteLocker isEnabledLocker( &isEnabledMutex_ );
	isEnabled_ = set;
}


bool ArchiveManagerPrivate::registerArchive( const ArchiveInstance & archiveInstance )
{
	QWriteLocker locker( &archivesMutex_ );

	const QString cleanMountPoint = archiveInstance.d->archive->cleanMountPointPath();

	if ( cleanMountPoint.isEmpty() )
	{
		qWarning() <<
			"Grim::ArchiveManager::registerArchive() : "
			"Mount point not exists.";
		return false;
	}

	if ( archiveForMountPoint_.contains( cleanMountPoint ) )
	{
		qWarning() <<
			"Grim::ArchiveManager::registerArchive() : "
			"Mount point already in use:" << cleanMountPoint;
		return false;
	}

	registeredArchives_ << archiveInstance;
	archiveForMountPoint_[ cleanMountPoint ] = archiveInstance;

	return true;
}


void ArchiveManagerPrivate::unregisterArchive( const ArchiveInstance & archiveInstance )
{
	QWriteLocker locker( &archivesMutex_ );

	const QString cleanMountPoint = archiveInstance.d->archive->cleanMountPointPath();

	Q_ASSERT( archiveForMountPoint_.contains( cleanMountPoint ) );

	registeredArchives_.removeOne( archiveInstance );
	archiveForMountPoint_.remove( cleanMountPoint );
}


/**
 * Looks up for compatible archive instance that handles \a cleanFilePath and returns
 * it with locked initialization mutex.
 */
ArchiveInstance ArchiveManagerPrivate::_findArchiveForFilePath( const QString & cleanFilePath )
{
	{
		// check if the given file name points right to archive
		ArchiveInstance archiveInstance = archiveForMountPoint_.value( cleanFilePath );
		if ( !archiveInstance.isNull() )
		{
			ArchivePrivate * archivePrivate = archiveInstance.d->archive;

			// check if archive was temporary disabled &&
			// check if archive should be visible as directory
			if ( !archiveThreadCache()->disabledArchives.contains( archiveInstance ) )
			{
				// archive is registered but can still be uninitialized, i.e. not opened
				archivePrivate->initializationMutex()->lockForRead();
				if ( archivePrivate->isInitialized() )
				{
					Q_ASSERT( archivePrivate->openMode() != Grim::Archive::NotOpen );
					if ( archivePrivate->treatAsDir() )
						return archiveInstance;
				}
				archivePrivate->initializationMutex()->unlock();
			}
		}
	}

	ArchiveInstance matchedArchiveInstance;
	QString matchedArchiveMountPoint;

	for ( QListIterator<ArchiveInstance> it( registeredArchives_ ); it.hasNext(); )
	{
		const ArchiveInstance & archiveInstance = it.next();
		ArchivePrivate * archivePrivate = archiveInstance.d->archive;

		if ( archiveThreadCache()->disabledArchives.contains( archiveInstance ) )
			continue;

		archivePrivate->initializationMutex()->lockForRead();

		if ( !archivePrivate->isInitialized() )
		{
			archivePrivate->initializationMutex()->unlock();
			continue;
		}

		const QString archiveMountPoint = archivePrivate->cleanMountPointPath();

		if ( !cleanFilePath.startsWith( archiveMountPoint ) )
		{
			archivePrivate->initializationMutex()->unlock();
			continue;
		}

		if ( archiveMountPoint == cleanFilePath )
		{
			// this archive was already checked upper, skip it
			archivePrivate->initializationMutex()->unlock();
			continue;
		}

		if ( !matchedArchiveInstance.isNull() && matchedArchiveMountPoint.length() > archiveMountPoint.length() )
		{
			// already found archive, which is better matched
			archivePrivate->initializationMutex()->unlock();
			continue;
		}

		if ( !matchedArchiveInstance.isNull() )
		{
			// unlock previous found archivePrivate instance
			matchedArchiveInstance.d->archive->initializationMutex()->unlock();
		}

		matchedArchiveInstance = archiveInstance;
		matchedArchiveMountPoint = archiveMountPoint;
	}

	return matchedArchiveInstance;
}


QAbstractFileEngine * ArchiveManagerPrivate::createFileEngine( const QString & fileName )
{
	if ( !isEnabled_ )
		return 0;

	if ( fileName.isEmpty() )
		return 0;

	if ( archiveThreadCache()->isManagerDisabled )
		return 0;

	archiveThreadCache()->isManagerDisabled = true;
	const QString absoluteFilePath = QFileInfo( fileName ).absoluteFilePath();
	archiveThreadCache()->isManagerDisabled = false;

	const bool isRelativePath = absoluteFilePath != fileName;

	QReadLocker locker( &archivesMutex_ );

	const QString cleanSoftFilePath = toSoftCleanPath( absoluteFilePath );
	const QString cleanHardFilePath = softToHardCleanPath( cleanSoftFilePath );

	ArchiveInstance archiveInstance = _findArchiveForFilePath( cleanHardFilePath );
	if ( archiveInstance.isNull() )
		return 0;

	QString internalFileName = cleanSoftFilePath.mid( archiveInstance.d->archive->cleanMountPointPath().length() + 1 );
	Q_ASSERT( !internalFileName.endsWith( QLatin1Char( '/' ) ) );
	Q_ASSERT( !internalFileName.startsWith( QLatin1Char( '/' ) ) );

	if ( internalFileName.isEmpty() )
	{
		// this is a root directory of archive, named '/'
		internalFileName = QLatin1String( "/" );
	}

	ArchiveFile * file = new ArchiveFile( archiveInstance, fileName, cleanSoftFilePath, internalFileName, isRelativePath );

	archiveInstance.d->archive->registerFile( file );

	// unlock archivePrivate state mutex
	archiveInstance.d->archive->initializationMutex()->unlock();

	return file;
}


ArchiveInstanceData * ArchiveManagerPrivate::sharedNullArchiveInstanceData()
{
	QWriteLocker locker( &sharedNullArchiveInstanceDataMutex_ );

	if ( !sharedNullArchiveInstanceData_ )
	{
		sharedNullArchiveInstanceData_ = new ArchiveInstanceData( 0 );
		sharedNullArchiveInstanceData_->ref.ref();
	}

	return sharedNullArchiveInstanceData_;
}




/**
 * \class ArchiveManager
 *
 * \ingroup archive_module
 *
 * \brief The ArchiveManager class is a singleton for managing global archive settings.
 *
 * The singleton is accessed with static sharedManager() method and allows to temporary turn off and on
 * all mounted points.
 */


/**
 * Returns archive manager singleton.
 */

ArchiveManager * ArchiveManager::sharedManager()
{
	QWriteLocker locker( &_sharedManagerMutex );

	if ( !_sharedManager )
	{
		_sharedManager = new ArchiveManager;
		qAddPostRoutine( ArchiveManagerPrivate::destroySharedManager );
	}

	return _sharedManager;
}


/** \internal
 */

ArchiveManager::ArchiveManager()
{
	d_ = new ArchiveManagerPrivate;
}


/** \internal
 */


ArchiveManager::~ArchiveManager()
{
	delete d_;
}


/**
 * Returns whether archive mount points are enabled globally.
 *
 * \sa setEnabled()
 */

bool ArchiveManager::isEnabled() const
{
	return d_->isEnabled();
}


/**
 * Globally turns mount on if \a set is \c true and off is \a set is \c false.
 *
 * \sa isEnabled()
 */

void ArchiveManager::setEnabled( bool set )
{
	d_->setEnabled( set );
}




} // namespace Grim
