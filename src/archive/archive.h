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

#include "archiveglobal.h"

#include <QObject>
#include <QStringList>




namespace Grim {




class ArchivePrivate;




class GRIM_ARCHIVE_EXPORT Archive : public QObject
{
	Q_OBJECT

public:
	Q_PROPERTY( OpenMode openMode READ openMode )
	Q_PROPERTY( QString fileName READ fileName WRITE setFileName )
	Q_PROPERTY( QString mountPoint READ mountPoint WRITE setMountPoint )
	Q_PROPERTY( QString actualMountPoint READ actualMountPoint )
	Q_PROPERTY( bool treatAsDir READ treatAsDir WRITE setTreatAsDir )

	enum OpenModeFlag
	{
		NotOpen     = 0x0000,
		ReadOnly    = 0x0001,
		WriteOnly   = 0x0002,
		ReadWrite   = ReadOnly | WriteOnly,
		DontLock    = 0x0004,
		Block       = 0x0008,
	};
	Q_DECLARE_FLAGS( OpenMode, OpenModeFlag )

	enum StateFlag
	{
		State_Idle         = 0x0000,
		State_Initializing = 0x0001,
		State_Ready        = 0x0002
	};
	Q_DECLARE_FLAGS( State, StateFlag )

	enum Type
	{
		Type_Unknown = 0,
		Type_Zip
	};

	Archive( QObject * parent = 0 );
	Archive( const QString & fileName, QObject * parent = 0 );
	~Archive();

	State state() const;

	bool isBroken() const;

	bool open( OpenMode openMode );
	void close();

	OpenMode openMode() const;

	QString fileName() const;
	void setFileName( const QString & fileName );

	QString mountPoint() const;
	void setMountPoint( const QString & mountPoint );

	QString actualMountPoint() const;

	bool treatAsDir() const;
	void setTreatAsDir( bool set );

	QString globalComment() const;

signals:
	void stateChanged( int state );

private:
	ArchivePrivate * d_;

	friend class ArchiveManagerPrivate;
	friend class ArchivePrivate;
	friend class ArchiveFile;
};




} // namespace Grim




Q_DECLARE_OPERATORS_FOR_FLAGS( Grim::Archive::OpenMode )
Q_DECLARE_OPERATORS_FOR_FLAGS( Grim::Archive::State )
