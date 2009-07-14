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

#pragma once

#include "archive_p.h"




namespace Grim {




class Archive;




class ArchiveFileEngineHandler : public QAbstractFileEngineHandler
{
public:
	QAbstractFileEngine * create( const QString & fileName ) const;
};




class ArchiveManagerPrivate
{
public:
	static ArchiveManagerPrivate * sharedManagerPrivate();

	static void destroySharedManager();

	ArchiveManagerPrivate();
	~ArchiveManagerPrivate();

	bool isEnabled() const;
	void setEnabled( bool set );

	bool registerArchive( const ArchiveInstance & archiveInstance );
	void unregisterArchive( const ArchiveInstance & archiveInstance );

	QAbstractFileEngine * createFileEngine( const QString & fileName );

	ArchiveInstanceData * sharedNullArchiveInstanceData();

private:
	ArchiveInstance _findArchiveForFilePath( const QString & cleanFilePath );

private:
	ArchiveFileEngineHandler * fileEngineHandler_;

	QReadWriteLock isEnabledMutex_;
	bool isEnabled_;

	QReadWriteLock archivesMutex_;
	QList<ArchiveInstance> registeredArchives_;
	QHash<QString,ArchiveInstance> archiveForMountPoint_;

	QReadWriteLock sharedNullArchiveInstanceDataMutex_;
	ArchiveInstanceData * sharedNullArchiveInstanceData_;
};




} // namespace Grim
