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

#include "pointsmodel.h"

#include <QFileIconProvider>
#include <QDir>

#include <grim/archive/archive.h>

#include "mainwindow.h"




PointsModelDriveItem::PointsModelDriveItem( const QFileInfo & info ) :
	PointsModelItem( PointsModel::PointType_Drive )
{
	QFileIconProvider iconProvider;

	icon_ = iconProvider.icon( info );
	path_ = info.absoluteFilePath();

	retranslateUi();
}


void PointsModelDriveItem::retranslateUi()
{
	QDir dir( path_ );
	if ( dir.isRoot() )
	{
		name_ = PointsModel::tr( "Root" );
	}
	else if ( dir.path() == QDir::homePath() )
	{
		name_ = PointsModel::tr( "Home" );
	}
	else
	{
		name_ = PointsModel::tr( "Drive" );
	}

	static const QString DisplayFormat = QLatin1String( "%1 ( %2 )" );
	displayText_ = DisplayFormat.arg( name_ ).arg( path_ );
}




PointsModelArchiveItem::PointsModelArchiveItem( ArchiveInfo * archiveInfo ) :
	PointsModelItem( PointsModel::PointType_Archive ),
	archiveInfo_( archiveInfo )
{
	fileName_ = QFileInfo( archiveInfo_->archiveFilePath ).fileName();
}




PointsModelPlayerItem::PointsModelPlayerItem( PlayerInfo * playerInfo ) :
	PointsModelItem( PointsModel::PointType_Player ),
	playerInfo_( playerInfo )
{
	fileName_ = QFileInfo( playerInfo_->filePath ).fileName();
}




PointsModelSeparatorItem::PointsModelSeparatorItem() :
	PointsModelItem( PointsModel::PointType_Separator )
{
}




PointsModel::PointsModel( QObject * parent ) :
	QAbstractItemModel( parent )
{
	QFont font;
	separatorSizeHint_ = QSize( 0, QFontMetrics( font ).height()*2 );

	// populate drives
	for ( QListIterator<QFileInfo> drivesIt( QDir::drives() ); drivesIt.hasNext(); )
		_addDrive( drivesIt.next() );
	_addDrive( QFileInfo( QDir::homePath() ) );

	// two separator items: above drives and archives
	drivesSeparatorItem_ = new PointsModelSeparatorItem;
	archivesSeparatorItem_ = new PointsModelSeparatorItem;
	playersSeparatorItem_ = new PointsModelSeparatorItem;

	retranslateUi();
}


PointsModel::~PointsModel()
{
	qDeleteAll( driveItems_ );
	qDeleteAll( archiveItems_ );
	qDeleteAll( playerItems_ );
	delete drivesSeparatorItem_;
	delete archivesSeparatorItem_;
	delete playersSeparatorItem_;
}


void PointsModel::_addDrive( const QFileInfo & info )
{
	const QString path = info.absoluteFilePath();

	if ( driveItemForPath_.contains( path ) )
		return;

	PointsModelDriveItem * driveItem = new PointsModelDriveItem( info );

	driveItems_ << driveItem;
	driveItemForPath_[ path ] = driveItem;
}


void PointsModel::addArchivePoint( ArchiveInfo * archiveInfo )
{
	const int rowToAdd = 2 + driveItems_.count() + archiveItems_.count();

	beginInsertRows( QModelIndex(), rowToAdd, rowToAdd );

	PointsModelArchiveItem * archiveItem = new PointsModelArchiveItem( archiveInfo );
	archiveItems_ << archiveItem;

	endInsertRows();
}


void PointsModel::removeArchivePoint( ArchiveInfo * archiveInfo )
{
	int row;
	for ( row = 0; row < archiveItems_.count(); ++row )
		if ( archiveItems_.at( row )->archiveInfo_ == archiveInfo )
			break;

	Q_ASSERT( row < archiveItems_.count() );

	const int rowToRemove = 2 + driveItems_.count() + row;

	beginRemoveRows( QModelIndex(), rowToRemove, rowToRemove );

	archiveItems_.removeAt( row );

	endRemoveRows();
}


void PointsModel::addPlayerPoint( PlayerInfo * playerInfo )
{
	const int rowToAdd = 3 + driveItems_.count() + archiveItems_.count() + playerItems_.count();

	beginInsertRows( QModelIndex(), rowToAdd, rowToAdd );

	PointsModelPlayerItem * playerItem = new PointsModelPlayerItem( playerInfo );
	playerItems_ << playerItem;

	endInsertRows();
}


void PointsModel::removePlayerPoint( PlayerInfo * playerInfo )
{
	int row;
	for ( row = 0; row < playerItems_.count(); ++row )
		if ( playerItems_.at( row )->playerInfo_ == playerInfo )
			break;

	Q_ASSERT( row < playerItems_.count() );

	const int rowToRemove = 3 + driveItems_.count() + archiveItems_.count() + row;

	beginRemoveRows( QModelIndex(), rowToRemove, rowToRemove );

	playerItems_.removeAt( row );

	endRemoveRows();
}


PointsModelItem * PointsModel::itemForRow( int row ) const
{
	if ( row == 0 )
		return drivesSeparatorItem_;
	row--;

	if ( row < driveItems_.count() )
		return driveItems_.at( row );
	row -= driveItems_.count();

	if ( row == 0 )
		return archivesSeparatorItem_;
	row--;

	if ( row < archiveItems_.count() )
		return archiveItems_.at( row );
	row -= archiveItems_.count();

	if ( row == 0 )
		return playersSeparatorItem_;
	row--;

	if ( row < playerItems_.count() )
		return playerItems_.at( row );
	row -= playerItems_.count();

	Q_ASSERT( false );

	return 0;
}


QString PointsModel::pathForIndex( const QModelIndex & index ) const
{
	Q_ASSERT( index.isValid() && index.column() == 0 && index.row() < rowCount( QModelIndex() ) );

	PointsModelItem * item = static_cast<PointsModelItem*>( index.internalPointer() );

	switch ( item->type_ )
	{
	case PointType_Drive:
		return static_cast<PointsModelDriveItem*>( item )->path_;

	case PointType_Archive:
	{
		PointsModelArchiveItem * archiveItem = static_cast<PointsModelArchiveItem*>( item );
		return archiveItem->archiveInfo_->archive ? archiveItem->archiveInfo_->archive->actualMountPoint() : QString();
	}

	case PointType_Player:
	{
		PointsModelPlayerItem * playerItem = static_cast<PointsModelPlayerItem*>( item );
		return playerItem->playerInfo_->filePath;
	}

	case PointType_Separator:
		return QString();
	}

	Q_ASSERT( false );
	return QString();
}


ArchiveInfo * PointsModel::archiveInfoForIndex( const QModelIndex & index ) const
{
	PointsModelItem * item = static_cast<PointsModelItem*>( index.internalPointer() );
	if ( item->type_ != PointType_Archive )
		return 0;
	return static_cast<PointsModelArchiveItem*>( item )->archiveInfo_;
}


PlayerInfo * PointsModel::playerInfoForIndex( const QModelIndex & index ) const
{
	PointsModelItem * item = static_cast<PointsModelItem*>( index.internalPointer() );
	if ( item->type_ != PointType_Player )
		return 0;
	return static_cast<PointsModelPlayerItem*>( item )->playerInfo_;
}


QModelIndex PointsModel::index( int row, int column, const QModelIndex & parent ) const
{
	if ( row < 0 || row >= rowCount( QModelIndex() ) || column != 0 || parent.isValid() )
		return QModelIndex();

	PointsModelItem * item = itemForRow( row );
	if ( !item )
		return QModelIndex();

	return createIndex( row, 0, (void*)item );
}


Qt::ItemFlags PointsModel::flags( const QModelIndex & index ) const
{
	if ( !index.isValid() || index.column() > 0 || index.row() >= rowCount( QModelIndex() ) )
		return 0;

	PointsModelItem * item = itemForRow( index.row() );

	if ( item->type_ == PointType_Drive || item->type_ == PointType_Archive || item->type_ == PointType_Player )
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;

	return 0;
}


QModelIndex PointsModel::parent( const QModelIndex & index ) const
{
	return QModelIndex();
}


int PointsModel::rowCount( const QModelIndex & index ) const
{
	if ( index.isValid() )
		return 0;

	return driveItems_.count() + archiveItems_.count() + playerItems_.count() + 3;
}


int PointsModel::columnCount( const QModelIndex & index ) const
{
	if ( index.isValid() )
		return 0;

	return 1;
}


QVariant PointsModel::data( const QModelIndex & index, int role ) const
{
	if ( !index.isValid() || index.column() > 0 || index.row() >= rowCount( QModelIndex() ) )
		return QVariant();

	PointsModelItem * item = static_cast<PointsModelItem*>( index.internalPointer() );

	switch ( item->type_ )
	{
	case PointType_Drive:
	{
		PointsModelDriveItem * driveItem = static_cast<PointsModelDriveItem*>( item );
		switch ( role )
		{
		case Qt::DisplayRole:
			return driveItem->displayText_;
		case Qt::DecorationRole:
			return driveItem->icon_;
		}
	}
		break;

	case PointType_Archive:
	{
		PointsModelArchiveItem * archiveItem = static_cast<PointsModelArchiveItem*>( item );
		switch ( role )
		{
		case Qt::DisplayRole:
			return archiveItem->fileName_;
		}
	}
		break;

	case PointType_Player:
	{
		PointsModelPlayerItem * playerItem = static_cast<PointsModelPlayerItem*>( item );
		switch ( role )
		{
		case Qt::DisplayRole:
			return playerItem->fileName_;
		}
	}
		break;

	case PointType_Separator:
	{
		PointsModelSeparatorItem * separatorItem = static_cast<PointsModelSeparatorItem*>( item );
		switch ( role )
		{
		case Qt::DisplayRole:
			return separatorItem->text_;
		case Qt::TextAlignmentRole:
			return Qt::AlignCenter;
		case Qt::FontRole:
		{
			QFont font;
			font.setItalic( true );
			return font;
		}
		case Qt::SizeHintRole:
			return separatorSizeHint_;
		}
	}
		break;
	}

	return QVariant();
}


void PointsModel::retranslateUi()
{
	drivesSeparatorItem_->text_ = tr( "Drives:" );
	archivesSeparatorItem_->text_ = tr( "Mounted archives:" );
	playersSeparatorItem_->text_ = tr( "Players:" );

	for ( QListIterator<PointsModelDriveItem*> it( driveItems_ ); it.hasNext(); )
		it.next()->retranslateUi();
}


bool PointsModel::event( QEvent * e )
{
	if ( e->type() == QEvent::LanguageChange )
		retranslateUi();

	return QAbstractItemModel::event( e );
}
