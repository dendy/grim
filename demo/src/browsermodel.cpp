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

#include "browsermodel.h"

#include "mainwindow.h"

#include <grim/archive/archive.h>
#include <grim/audio/audiosource.h>




BrowserModel::BrowserModel( MainWindow * mainWindow ) :
	QFileSystemModel( mainWindow ),
	mainWindow_( mainWindow )
{

}


int BrowserModel::_toParentColumn( int column ) const
{
	return column + QFileSystemModel::columnCount();
}


int BrowserModel::_fromParentColumn( int column ) const
{
	return column - QFileSystemModel::columnCount();
}


QModelIndex BrowserModel::firstIndex( const QModelIndex & index ) const
{
	if ( !index.isValid() )
		return QModelIndex();
	return this->index( index.row(), 0, index.parent() );
}


QModelIndex BrowserModel::lastIndex( const QModelIndex & index ) const
{
	if ( !index.isValid() )
		return QModelIndex();
	return this->index( index.row(), columnCount( QModelIndex() ) - 1, index.parent() );
}


QModelIndex BrowserModel::index( int row, int column, const QModelIndex & parent ) const
{
	const int ourColumn = _fromParentColumn( column );
	if ( ourColumn < 0 )
		return QFileSystemModel::index( row, column, parent );

	return createIndex( row, column, index( row, 0, parent ).internalPointer() );
}


QModelIndex BrowserModel::parent( const QModelIndex & index ) const
{
	const int ourColumn = _fromParentColumn( index.column() );
	if ( ourColumn < 0 )
		return QFileSystemModel::parent( index );

	return QFileSystemModel::parent( createIndex( index.row(), 0, index.internalPointer() ) );
}


int BrowserModel::columnCount( const QModelIndex & index ) const
{
	int columns = QFileSystemModel::columnCount( index );
	return columns == 0 ? 0 : columns + TotalColumns;
}


Qt::ItemFlags BrowserModel::flags( const QModelIndex & index ) const
{
	const int ourColumn = _fromParentColumn( index.column() );
	if ( ourColumn < 0 )
		return QFileSystemModel::flags( index );

	switch ( ourColumn )
	{
	case Column_Status:
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	return 0;
}


QVariant BrowserModel::data( const QModelIndex & index, int role ) const
{
	const int ourColumn = _fromParentColumn( index.column() );
	if ( ourColumn < 0 )
		return QFileSystemModel::data( index, role );

	switch ( ourColumn )
	{
	case Column_Status:
	{
		const QModelIndex firstIndex = this->firstIndex( index );

		switch ( role )
		{
		case Qt::DisplayRole:
		{
			// check if this index points to mounted archive
			ArchiveInfo * archiveInfo = mainWindow_->archiveInfoForArchiveIndex_.value( firstIndex );
			if ( archiveInfo )
			{
				if ( archiveInfo->isBroken )
					return tr( "Broken" );
				Q_ASSERT( archiveInfo->archive );
				if ( archiveInfo->archive->state() == Grim::Archive::State_Ready )
					return tr( "Mounted" );
				if ( archiveInfo->archive->state() == Grim::Archive::State_Initializing )
					return tr( "Mounting..." );
				return tr( "Idle" );
			}

			// check if this index points to playing file
			PlayerInfo * playerInfo = mainWindow_->playerInfoForIndex_.value( firstIndex );
			if ( playerInfo )
			{
				if ( !playerInfo->audioSource )
					return tr( "Voided" );

				switch ( playerInfo->audioSource->state() )
				{
				case Grim::AudioSource::State_Playing:
					return tr( "Playing" );
				case Grim::AudioSource::State_Paused:
					return tr( "Paused" );
				case Grim::AudioSource::State_Stopped:
					return tr( "Stopped" );
				return tr( "Idle" );
				}
			}
		}
			break;
		}
	}
		break;
	}

	return QVariant();
}


QVariant BrowserModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
	if ( orientation == Qt::Vertical )
		return QFileSystemModel::headerData( section, orientation, role );

	const int ourColumn = _fromParentColumn( section );
	if ( ourColumn < 0 )
		return QFileSystemModel::headerData( section, orientation, role );

	switch ( ourColumn )
	{
	case Column_Status:
	{
		if ( role == Qt::DisplayRole )
			return tr( "Status" );
	}
		break;
	}

	return QVariant();
}


bool BrowserModel::event( QEvent * e )
{
	if ( e->type() == QEvent::LanguageChange )
	{
		return QFileSystemModel::event( e );
	}

	return QFileSystemModel::event( e );
}
