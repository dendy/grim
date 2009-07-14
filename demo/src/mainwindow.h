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

#include <QMainWindow>
#include <QBasicTimer>

#include "ui_mainwindow.h"
#include "ui_browserwidget.h"




class QFile;

namespace Grim {
	class Archive;
	class AudioSource;
	class FpsCounter;
}

class PointsModel;
class BrowserModel;
class Player;
class AudioWidget;
class PlayerWidget;
class ArchiveTest;




class PlayerInfo
{
public:
	QString filePath;
	Grim::AudioSource * audioSource;
	QPersistentModelIndex index;
	Player * player;
};

Q_DECLARE_METATYPE(PlayerInfo*)




class ArchiveInfo
{
public:
	ArchiveInfo() : archive( 0 ) {}

	bool isBroken;
	QString archiveFilePath;
	QString mountPointPath;
	QPersistentModelIndex archiveIndex;
	QPersistentModelIndex mountPointIndex;
	Grim::Archive * archive;
	QFile * mountPointFile;
};




class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	enum LayoutMode
	{
		LayoutMode_Invalid = 0,
		LayoutMode_Single,
		LayoutMode_Separate
	};

	MainWindow();
	~MainWindow();

	ArchiveInfo * mountArchive( const QModelIndex & index );
	void unmountArchive( ArchiveInfo * archiveInfo );

	PlayerInfo * createPlayer( const QModelIndex & index );
	void destroyPlayer( PlayerInfo * playerInfo );

	void killAudioSources();
	void reviveAudioSources( const QList<PlayerInfo*> & playerInfos );

	QStringList mountPoints() const;

	void audioContextCreated();
	void audioContextDestroyed();

	void retranslateUi();

protected:
	void changeEvent( QEvent * e );
	void timerEvent( QTimerEvent * e );
	bool eventFilter( QObject * o, QEvent * e );

private:
	void _createBrowserWidget();
	void _createAudioWidget();
	void _createSinglePlayerWidget();

	void _switchToLayoutMode( LayoutMode layoutMode );

	void _populateArchiveMenu( const QList<QModelIndex> & indexes );
	void _populatePlayerMenu( const QList<QModelIndex> & indexes );

	void _destroyArchiveInfo( ArchiveInfo * archiveInfo );
	void _destroyPlayerInfo( PlayerInfo * playerInfo );

	void _updateBrowserIndex( const QModelIndex & index );
	void _scrollToArchiveContents( ArchiveInfo * archiveInfo );
	void _showPlayer( PlayerInfo * playerInfo );
	PlayerWidget * _playerWidgetForPlayerInfo( PlayerInfo * playerInfo );

private slots:
	void on_menuView_aboutToShow();
	void on_actionSingleLayoutMode_triggered();
	void on_actionSeparateLayoutMode_triggered();

	void on_menuArchive_aboutToShow();
	void on_actionMountArchive_triggered();
	void on_actionUnmountArchive_triggered();
	void on_actionUnmountAllArchives_triggered();

	void on_menuPlayer_aboutToShow();
	void on_actionShowPlayer_triggered();
	void on_actionCreatePlayer_triggered();
	void on_actionRecreatePlayer_triggered();
	void on_actionDestroyPlayer_triggered();
	void on_actionSourcePlay_triggered();
	void on_actionSourcePause_triggered();
	void on_actionSourceStop_triggered();
	void on_actionSourceLoop_toggled( bool on );

	void on_menuWindow_aboutToShow();
	void _archiveTestTriggered();
	void _audioWindowTriggered();
	void _playerWindowTriggered();

	void on_menuSettings_aboutToShow();
	void _languageTriggered();

	void on_actionAbout_triggered();

	void _pointActivated( const QModelIndex & index );
	void _browserActivated( const QModelIndex & index );

	void _archiveStateChanged();

	void _sourceInitializationChanged( Grim::AudioSource * audioSource );
	void _sourceStateChanged( Grim::AudioSource * audioSource );
	void _sourceCurrentOffsetChanged( Grim::AudioSource * audioSource );

private:
	Ui_MainWindow ui_;

	// languages
	bool languagesPopulated_;

	// fps counter
	Grim::FpsCounter * fpsCounter_;
	QBasicTimer fpsTimer_;

	// layout mode
	LayoutMode layoutMode_;

	// browser widget
	QWidget * browserWidget_;
	Ui_BrowserWidget browserUi_;
	PointsModel * pointsModel_;
	BrowserModel * browserModel_;
	QListView * pointsView_;
	QTreeView * browserView_;

	// audio widget
	AudioWidget * audioWidget_;

	// single player widget
	PlayerWidget * singlePlayerWidget_;
	PlayerInfo * singlePlayerInfo_;

	// archive test dialog
	ArchiveTest * archiveTest_;

	// selected indexes
	QList<QPersistentModelIndex> selectedArchiveIndexes_;
	QPersistentModelIndex selectedPlayerIndex_;

	// archives
	QList<ArchiveInfo*> archiveInfos_;
	QHash<QString,ArchiveInfo*> archiveInfoForArchiveFilePath_;
	QHash<QPersistentModelIndex,ArchiveInfo*> archiveInfoForArchiveIndex_;
	QHash<QString,ArchiveInfo*> archiveInfoForMountPointPath_;
	QHash<QPersistentModelIndex,ArchiveInfo*> archiveInfoForMountPointIndex_;
	QHash<Grim::Archive*,ArchiveInfo*> archiveInfoForArchive_;

	// players
	QList<PlayerInfo*> playerInfos_;
	QHash<QString,PlayerInfo*> playerInfoForFilePath_;
	QHash<QPersistentModelIndex,PlayerInfo*> playerInfoForIndex_;
	QHash<Grim::AudioSource*,PlayerInfo*> playerInfoForSource_;

	friend class BrowserModel;
};
