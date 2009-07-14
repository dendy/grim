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

#include <QAbstractItemModel>
#include <QIcon>




class QFileInfo;

class ArchiveInfo;
class PlayerInfo;




class PointsModelItem
{
public:
	PointsModelItem( int type ) : type_( type ) {}
	virtual ~PointsModelItem() {}

private:
	int type_;

	friend class PointsModel;
};




class PointsModelDriveItem : public PointsModelItem
{
public:
	PointsModelDriveItem( const QFileInfo & info );

	void retranslateUi();

private:
	QString path_;
	QIcon icon_;
	QString name_;

	QString displayText_;

	friend class PointsModel;
};




class PointsModelArchiveItem : public PointsModelItem
{
public:
	PointsModelArchiveItem( ArchiveInfo * archiveInfo );

private:
	ArchiveInfo * archiveInfo_;
	QString fileName_;

	friend class PointsModel;
};




class PointsModelPlayerItem : public PointsModelItem
{
public:
	PointsModelPlayerItem( PlayerInfo * playerInfo );

private:
	PlayerInfo * playerInfo_;
	QString fileName_;

	friend class PointsModel;
};




class PointsModelSeparatorItem : public PointsModelItem
{
public:
	PointsModelSeparatorItem();

private:
	QString text_;

	friend class PointsModel;
};




class PointsModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum PointType
	{
		PointType_Invalid = 0,
		PointType_Drive,
		PointType_Archive,
		PointType_Player,
		PointType_Separator
	};

	PointsModel( QObject * parent = 0 );
	~PointsModel();

	void addArchivePoint( ArchiveInfo * archiveInfo );
	void removeArchivePoint( ArchiveInfo * archiveInfo );

	void addPlayerPoint( PlayerInfo * playerInfo );
	void removePlayerPoint( PlayerInfo * playerInfo );

	PointsModelItem * itemForRow( int row ) const;
	QString pathForIndex( const QModelIndex & index ) const;
	ArchiveInfo * archiveInfoForIndex( const QModelIndex & index ) const;
	PlayerInfo * playerInfoForIndex( const QModelIndex & index ) const;

	QModelIndex index( int row, int column, const QModelIndex & parent ) const;
	Qt::ItemFlags flags( const QModelIndex & index ) const;
	QModelIndex parent( const QModelIndex & index ) const;
	int rowCount( const QModelIndex & index ) const;
	int columnCount( const QModelIndex & index ) const;
	QVariant data( const QModelIndex & index, int role ) const;

	void retranslateUi();

protected:
	bool event( QEvent * e );

private:
	void _addDrive( const QFileInfo & info );

private:
	QHash<QString,PointsModelDriveItem*> driveItemForPath_;
	QList<PointsModelDriveItem*> driveItems_;
	QList<PointsModelArchiveItem*> archiveItems_;
	QList<PointsModelPlayerItem*> playerItems_;
	PointsModelSeparatorItem * drivesSeparatorItem_;
	PointsModelSeparatorItem * archivesSeparatorItem_;
	PointsModelSeparatorItem * playersSeparatorItem_;

	QSize separatorSizeHint_;
};
