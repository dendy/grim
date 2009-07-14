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

#include "mainwindow.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QTemporaryFile>

#include <grim/archive/archive.h>
#include <grim/audio/audiomanager.h>
#include <grim/audio/audiobuffer.h>
#include <grim/audio/audiosource.h>
#include <grim/audio/audiocontext.h>
#include <grim/tools/fpscounter.h>

#include "browsermodel.h"
#include "pointsmodel.h"
#include "audiowidget.h"
#include "playerwidget.h"
#include "player.h"
#include "archivetest.h"
#include "languagemanager.h"

#include "ui_aboutdialog.h"




static const MainWindow::LayoutMode DefaultLayoutMode =
#ifdef Q_WS_MAC
	MainWindow::LayoutMode_Separate;
#else
	MainWindow::LayoutMode_Single;
#endif




MainWindow::MainWindow()
{
	ui_.setupUi( this );

	languagesPopulated_ = false;

	_createBrowserWidget();
	_createAudioWidget();
	_createSinglePlayerWidget();

	archiveTest_ = 0;

	// frames per second counter
	fpsCounter_ = new Grim::FpsCounter( this );
	fpsTimer_.start( 0, this );

	layoutMode_ = LayoutMode_Invalid;
	_switchToLayoutMode( DefaultLayoutMode );

	// action group for layout modes
	{
		QActionGroup * layoutModesActionGroup = new QActionGroup( this );
		layoutModesActionGroup->addAction( ui_.actionSingleLayoutMode );
		layoutModesActionGroup->addAction( ui_.actionSeparateLayoutMode );
	}

	// default connections
	connect( ui_.actionQuit, SIGNAL(triggered()), QApplication::instance(), SLOT(quit()) );
	connect( ui_.actionAboutQt, SIGNAL(triggered()), QApplication::instance(), SLOT(aboutQt()) );

	audioWidget_->initializeAudio();

	retranslateUi();

#if 0
	const QString archiveFileName = "sounds.zip";
	Grim::Archive archive( archiveFileName );
	archive.open( Grim::Archive::DontLock | Grim::Archive::ReadOnly );
//	ArchiveInfo * ai = mountArchive( browserModel_->QFileSystemModel::index( archiveFileName ) );
	for ( QDirIterator it( archiveFileName, QDirIterator::Subdirectories ); it.hasNext(); )
		qDebug() << it.next();
#endif
}


MainWindow::~MainWindow()
{
	// destroy all mounted archives
	{
		for ( QListIterator<ArchiveInfo*> it( archiveInfos_ ); it.hasNext(); )
			_destroyArchiveInfo( it.next() );
		qDeleteAll( archiveInfos_ );

		archiveInfos_.clear();
		archiveInfoForArchiveFilePath_.clear();
		archiveInfoForArchiveIndex_.clear();
		archiveInfoForMountPointPath_.clear();
		archiveInfoForMountPointIndex_.clear();
		archiveInfoForArchive_.clear();
	}

	// destroy all players
	{
		singlePlayerWidget_->setPlayerInfo( 0 );
		singlePlayerInfo_ = 0;

		for ( QListIterator<PlayerInfo*> it( playerInfos_ ); it.hasNext(); )
			_destroyPlayerInfo( it.next() );
		qDeleteAll( playerInfos_ );

		playerInfos_.clear();
		playerInfoForFilePath_.clear();
		playerInfoForIndex_.clear();
		playerInfoForSource_.clear();
	}
}


void MainWindow::changeEvent( QEvent * e )
{
	if ( e->type() == QEvent::LanguageChange )
	{
		ui_.retranslateUi( this );
		browserUi_.retranslateUi( browserWidget_ );
		retranslateUi();
	}

	QMainWindow::changeEvent( e );
}


void MainWindow::timerEvent( QTimerEvent * e )
{
	if ( e->timerId() == fpsTimer_.timerId() )
	{
		browserUi_.labelFps->setText( QString::number( (double)fpsCounter_->hit(), 'f', 2 ) );
		return;
	}

	return QMainWindow::timerEvent( e );
}


bool MainWindow::eventFilter( QObject * o, QEvent * e )
{
	if ( o == pointsView_ && e->type() == QEvent::ContextMenu )
	{
		QContextMenuEvent * ce = static_cast<QContextMenuEvent*>( e );

		if ( !pointsView_->selectionModel()->hasSelection() )
			return true;

		int archivesFound = 0;
		int playersFound = 0;
		for ( QListIterator<QModelIndex> it( pointsView_->selectionModel()->selectedRows() ); it.hasNext(); )
		{
			const QModelIndex index = it.next();
			if ( pointsModel_->archiveInfoForIndex( index ) )
			{
				archivesFound++;
				continue;
			}
			if ( pointsModel_->playerInfoForIndex( index ) )
			{
				playersFound++;
				continue;
			}
		}

		if ( archivesFound > 0 && playersFound > 0 )
		{
			// mixed selection of archives and players, do nothing
			return true;
		}

		if ( archivesFound > 0 )
		{
			ui_.menuArchive->exec( ce->globalPos() );
			return true;
		}

		if ( playersFound > 0 )
		{
			ui_.menuPlayer->exec( ce->globalPos() );
			return true;
		}

		return true;
	}

	if ( o == browserView_ && e->type() == QEvent::ContextMenu )
	{
		QContextMenuEvent * ce = static_cast<QContextMenuEvent*>( e );

		QModelIndex index = browserModel_->firstIndex( browserView_->currentIndex() );
		if ( !index.isValid() )
			return true;

		const QFileInfo fileInfo = browserModel_->fileInfo( index );

		// check if this file can be an audio file to play
		if ( playerInfoForIndex_.contains( index ) ||
			Grim::AudioManager::sharedManager()-> availableFileFormatExtensions().contains( fileInfo.suffix().toLower() ) )
		{
			ui_.menuPlayer->exec( ce->globalPos() );
			return true;
		}

		// check if this file(s) can be an archive to mount/unmount
		if ( archiveInfoForArchiveIndex_.value( index ) || archiveInfoForMountPointIndex_.value( index ) ||
			fileInfo.suffix().toLower() == QLatin1String( "zip" ) )
		{
			ui_.menuArchive->exec( ce->globalPos() );
			return true;
		}

		return true;
	}

	return QMainWindow::eventFilter( 0, e );
}


void MainWindow::_createBrowserWidget()
{
	browserWidget_ = new QWidget;
	browserUi_.setupUi( browserWidget_ );

	// less shorter accessors
	pointsView_ = browserUi_.viewPoints;
	browserView_ = browserUi_.viewBrowser;

	// points model+view
	pointsModel_ = new PointsModel( this );
	pointsView_->setModel( pointsModel_ );
	pointsView_->installEventFilter( this );
	connect( pointsView_, SIGNAL(activated(QModelIndex)), SLOT(_pointActivated(QModelIndex)) );

	// browser model+view
	browserModel_ = new BrowserModel( this );
	browserView_->setModel( browserModel_ );
	browserView_->installEventFilter( this );
	connect( browserView_, SIGNAL(activated(QModelIndex)), SLOT(_browserActivated(QModelIndex)) );

	// hide useless columns
	browserView_->hideColumn( 1 ); // size
	browserView_->hideColumn( 2 ); // type
	browserView_->hideColumn( 3 ); // modification time

	browserView_->header()->setResizeMode( 0, QHeaderView::ResizeToContents );

	const QModelIndex rootIndex = browserModel_->setRootPath( QDir::currentPath() );
	for ( QModelIndex index = rootIndex; index.isValid(); index = index.parent() )
		browserView_->setExpanded( index, true );

	browserView_->scrollTo( rootIndex, QAbstractItemView::PositionAtTop );
	browserView_->setCurrentIndex( rootIndex );
}


void MainWindow::_createAudioWidget()
{
	audioWidget_ = new AudioWidget( this );
}


void MainWindow::_createSinglePlayerWidget()
{
	singlePlayerWidget_ = new PlayerWidget( 0, this );
	singlePlayerInfo_ = 0;
}


void MainWindow::_switchToLayoutMode( LayoutMode layoutMode )
{
	if ( layoutMode == layoutMode_ )
		return;

	layoutMode_ = layoutMode;

	if ( centralWidget()->layout() )
		delete centralWidget()->layout();

	if ( layoutMode_ == LayoutMode_Single )
	{
		QHBoxLayout * mainLayout = new QHBoxLayout( centralWidget() );

		QVBoxLayout * sideLayout = new QVBoxLayout;
		sideLayout->addWidget( audioWidget_ );
		sideLayout->addSpacerItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding ) );
		sideLayout->addWidget( singlePlayerWidget_ );

		mainLayout->addWidget( browserWidget_ );
		mainLayout->addLayout( sideLayout );

		{
			for ( QListIterator<PlayerInfo*> it( playerInfos_ ); it.hasNext(); )
			{
				PlayerInfo * playerInfo = it.next();
				if ( playerInfo->player )
				{
					delete playerInfo->player;
					playerInfo->player = 0;
				}
			}
		}

		if ( singlePlayerInfo_ )
			singlePlayerWidget_->setPlayerInfo( singlePlayerInfo_ );

		singlePlayerWidget_->show();
		audioWidget_->show();
	}
	else
	{
		singlePlayerWidget_->hide();

		// main window
		{
			QHBoxLayout * mainLayout = new QHBoxLayout( centralWidget() );
			mainLayout->addWidget( browserWidget_ );
		}

		// audio window
		{
			audioWidget_->setParent( this, audioWidget_->windowFlags() | Qt::Dialog );
			audioWidget_->show();
			audioWidget_->raise();
		}

		// player windows
		{
			for ( QListIterator<PlayerInfo*> it( playerInfos_ ); it.hasNext(); )
			{
				PlayerInfo * playerInfo = it.next();
				Q_ASSERT( !playerInfo->player );

				playerInfo->player = new Player( playerInfo, this );
				playerInfo->player->show();
			}
		}
	}
}


void MainWindow::_populateArchiveMenu( const QList<QModelIndex> & indexes )
{
	int mountedCount = 0;
	int unmountedCount = 0;
	selectedArchiveIndexes_.clear();
	for ( QListIterator<QModelIndex> it( indexes ); it.hasNext(); )
	{
		const QModelIndex selectedIndex = browserModel_->firstIndex( it.next() );
		if ( !selectedIndex.isValid() )
			continue;

		ArchiveInfo * archiveInfo = archiveInfoForArchiveIndex_.value( selectedIndex );
		if ( !archiveInfo )
			archiveInfo = archiveInfoForMountPointIndex_.value( selectedIndex );

		if ( archiveInfo )
			mountedCount++;
		else
			unmountedCount++;

		selectedArchiveIndexes_ << (archiveInfo ? archiveInfo->archiveIndex : QPersistentModelIndex( selectedIndex ));
	}

	ui_.actionMountArchive->setEnabled( mountedCount == 0 && unmountedCount == 1 );
	ui_.actionUnmountArchive->setEnabled( mountedCount == 1 && unmountedCount == 0 );
	ui_.actionUnmountAllArchives->setEnabled( mountedCount + unmountedCount > 1 && mountedCount > 0 );
}


void MainWindow::_populatePlayerMenu( const QList<QModelIndex> & indexes )
{
	if ( indexes.isEmpty() )
	{
		ui_.actionCreatePlayer->setVisible( true );
		ui_.actionRecreatePlayer->setVisible( false );

		ui_.actionCreatePlayer->setEnabled( false );
		ui_.actionRecreatePlayer->setEnabled( false );
		ui_.actionDestroyPlayer->setEnabled( false );
		ui_.actionShowPlayer->setEnabled( false );

		ui_.actionSourcePlay->setEnabled( false );
		ui_.actionSourcePause->setEnabled( false );
		ui_.actionSourceStop->setEnabled( false );
		ui_.actionSourceLoop->setEnabled( false );

		return;
	}

	if ( indexes.count() > 1 )
	{
		// ?
	}

	selectedPlayerIndex_ = indexes.first();

	PlayerInfo * playerInfo = 0;
	bool extensionMatched = false;
	if ( selectedPlayerIndex_.isValid() )
	{
		playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
		if ( !playerInfo )
		{
			const QFileInfo fileInfo = browserModel_->fileInfo( selectedPlayerIndex_ );
			extensionMatched = Grim::AudioManager::sharedManager()->
				availableFileFormatExtensions().contains( fileInfo.suffix().toLower() );
		}
	}

	ui_.actionSourceLoop->blockSignals( true );
	ui_.actionSourceLoop->setChecked( playerInfo && playerInfo->audioSource && playerInfo->audioSource->isLooping() );
	ui_.actionSourceLoop->blockSignals( false );

	if ( !playerInfo )
	{
		ui_.actionSourcePlay->setEnabled( false );
		ui_.actionSourcePause->setEnabled( false );
		ui_.actionSourceStop->setEnabled( false );
		ui_.actionSourceLoop->setEnabled( false );

		ui_.actionCreatePlayer->setVisible( true );
		ui_.actionRecreatePlayer->setVisible( false );

		ui_.actionCreatePlayer->setEnabled( extensionMatched );
		ui_.actionShowPlayer->setEnabled( false );
		ui_.actionDestroyPlayer->setEnabled( false );
	}
	else
	{
		ui_.actionCreatePlayer->setEnabled( false );
		ui_.actionShowPlayer->setEnabled( true );
		ui_.actionDestroyPlayer->setEnabled( true );

		if ( playerInfo->audioSource )
		{
			ui_.actionSourcePlay->setEnabled( true );
			ui_.actionSourcePause->setEnabled( true );
			ui_.actionSourceStop->setEnabled( true );
			ui_.actionSourceLoop->setEnabled( true );

			ui_.actionRecreatePlayer->setVisible( false );
			ui_.actionCreatePlayer->setVisible( true );
		}
		else
		{
			ui_.actionSourcePlay->setEnabled( false );
			ui_.actionSourcePause->setEnabled( false );
			ui_.actionSourceStop->setEnabled( false );
			ui_.actionSourceLoop->setEnabled( false );

			ui_.actionRecreatePlayer->setVisible( true );
			ui_.actionCreatePlayer->setVisible( false );
		}
	}

	ui_.actionRecreatePlayer->setEnabled( audioWidget_->hasAudio() );
}


void MainWindow::_destroyArchiveInfo( ArchiveInfo * archiveInfo )
{
	if ( archiveInfo->archive )
		delete archiveInfo->archive;

	if ( archiveInfo->mountPointFile )
		delete archiveInfo->mountPointFile;
}


void MainWindow::_destroyPlayerInfo( PlayerInfo * playerInfo )
{
	if ( playerInfo->player )
		delete playerInfo->player;

	if ( playerInfo->audioSource )
		delete playerInfo->audioSource;
}


void MainWindow::_updateBrowserIndex( const QModelIndex & index )
{
	const QModelIndex firstIndex = browserModel_->firstIndex( index );
	const QModelIndex lastIndex = browserModel_->lastIndex( index );
	emit browserModel_->dataChanged( firstIndex, lastIndex );
}


void MainWindow::_scrollToArchiveContents( ArchiveInfo * archiveInfo )
{
	if ( !archiveInfo->archive )
		return;

	browserView_->setExpanded( archiveInfo->mountPointIndex, true );
	browserView_->scrollTo( archiveInfo->mountPointIndex, QAbstractItemView::PositionAtTop );
	browserView_->setCurrentIndex( archiveInfo->mountPointIndex );
}


void MainWindow::_showPlayer( PlayerInfo * playerInfo )
{
	singlePlayerInfo_ = playerInfo;

	switch ( layoutMode_ )
	{
	case LayoutMode_Single:
	{
		singlePlayerWidget_->setPlayerInfo( playerInfo );
	}
		return;

	case LayoutMode_Separate:
	{
		if ( !playerInfo->player )
			playerInfo->player = new Player( playerInfo, this );
		playerInfo->player->show();
		playerInfo->player->activateWindow();
	}
		return;
	}
}


PlayerWidget * MainWindow::_playerWidgetForPlayerInfo( PlayerInfo * playerInfo )
{
	if ( playerInfo->player )
		return playerInfo->player->playerWidget();

	if ( playerInfo == singlePlayerInfo_ )
		return singlePlayerWidget_;

	return 0;
}


void MainWindow::on_menuView_aboutToShow()
{
	(layoutMode_ == LayoutMode_Single ? ui_.actionSingleLayoutMode : ui_.actionSeparateLayoutMode)->
		setChecked( true );
}


void MainWindow::on_actionSingleLayoutMode_triggered()
{
	_switchToLayoutMode( LayoutMode_Single );
}


void MainWindow::on_actionSeparateLayoutMode_triggered()
{
	_switchToLayoutMode( LayoutMode_Separate );
}


void MainWindow::on_menuArchive_aboutToShow()
{
	QList<QModelIndex> indexes;

	if ( pointsView_->hasFocus() )
	{
		for ( QListIterator<QModelIndex> it( pointsView_->selectionModel()->selectedRows() ); it.hasNext(); )
		{
			const QModelIndex index = it.next();
			ArchiveInfo * archiveInfo = pointsModel_->archiveInfoForIndex( index );
			if ( !archiveInfo )
				continue;
			indexes << archiveInfo->archiveIndex;
		}
	}
	else
	{
		indexes = browserView_->selectionModel()->selectedRows();
	}

	_populateArchiveMenu( indexes );
}


void MainWindow::on_actionMountArchive_triggered()
{
	Q_ASSERT( selectedArchiveIndexes_.count() == 1 );

	const QModelIndex index = selectedArchiveIndexes_.first();
	selectedArchiveIndexes_.clear();

	ArchiveInfo * archiveInfo = archiveInfoForArchiveIndex_.value( index );
	Q_ASSERT( !archiveInfo );

	archiveInfo = mountArchive( index );
}


void MainWindow::on_actionUnmountArchive_triggered()
{
	Q_ASSERT( selectedArchiveIndexes_.count() == 1 );

	const QModelIndex index = selectedArchiveIndexes_.first();
	selectedArchiveIndexes_.clear();

	ArchiveInfo * archiveInfo = archiveInfoForArchiveIndex_.value( index );
	Q_ASSERT( archiveInfo );

	unmountArchive( archiveInfo );
}


void MainWindow::on_actionUnmountAllArchives_triggered()
{
	for ( QListIterator<QPersistentModelIndex> it( selectedArchiveIndexes_ ); it.hasNext(); )
	{
		const QPersistentModelIndex selectedIndex = it.next();
		if ( !selectedIndex.isValid() )
			continue;

		ArchiveInfo * archiveInfo = archiveInfoForArchiveIndex_.value( selectedIndex );
		if ( archiveInfo )
			unmountArchive( archiveInfo );
	}

	selectedArchiveIndexes_.clear();
}


void MainWindow::on_menuPlayer_aboutToShow()
{
	QList<QModelIndex> indexes;

	if ( pointsView_->hasFocus() )
	{
		for ( QListIterator<QModelIndex> it( pointsView_->selectionModel()->selectedRows() ); it.hasNext(); )
		{
			const QModelIndex index = it.next();
			PlayerInfo * playerInfo = pointsModel_->playerInfoForIndex( index );
			if ( !playerInfo )
				continue;
			indexes << playerInfo->index;
		}
	}
	else
	{
		indexes = browserView_->selectionModel()->selectedRows();
	}

	_populatePlayerMenu( indexes );
}


void MainWindow::on_actionShowPlayer_triggered()
{
	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
	selectedPlayerIndex_ = QModelIndex();

	Q_ASSERT( playerInfo );

	_showPlayer( playerInfo );
}


void MainWindow::on_actionCreatePlayer_triggered()
{
	if ( !audioWidget_->hasAudio() )
		return;

	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );

	if ( !playerInfo )
		playerInfo = createPlayer( selectedPlayerIndex_ );

	selectedPlayerIndex_ = QModelIndex();

	if ( !playerInfo )
		return;

	_showPlayer( playerInfo );
}


void MainWindow::on_actionRecreatePlayer_triggered()
{
	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
	selectedPlayerIndex_ = QModelIndex();

	Q_ASSERT( playerInfo );
	Q_ASSERT( !playerInfo->audioSource );

	if ( audioWidget_->hasAudio() )
		reviveAudioSources( QList<PlayerInfo*>() << playerInfo );
}


void MainWindow::on_actionDestroyPlayer_triggered()
{
	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
	selectedPlayerIndex_ = QModelIndex();

	Q_ASSERT( playerInfo );

	destroyPlayer( playerInfo );
}


void MainWindow::on_actionSourcePlay_triggered()
{
	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
	selectedPlayerIndex_ = QModelIndex();

	Q_ASSERT( playerInfo );
	Q_ASSERT( playerInfo->audioSource );

	playerInfo->audioSource->play();
}


void MainWindow::on_actionSourcePause_triggered()
{
	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
	selectedPlayerIndex_ = QModelIndex();

	Q_ASSERT( playerInfo );
	Q_ASSERT( playerInfo->audioSource );

	playerInfo->audioSource->pause();
}


void MainWindow::on_actionSourceStop_triggered()
{
	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
	selectedPlayerIndex_ = QModelIndex();

	Q_ASSERT( playerInfo );
	Q_ASSERT( playerInfo->audioSource );

	playerInfo->audioSource->stop();
}


void MainWindow::on_actionSourceLoop_toggled( bool on )
{
	if ( !selectedPlayerIndex_.isValid() )
		return;

	PlayerInfo * playerInfo = playerInfoForIndex_.value( selectedPlayerIndex_ );
	selectedPlayerIndex_ = QModelIndex();

	Q_ASSERT( playerInfo );
	Q_ASSERT( playerInfo->audioSource );

	playerInfo->audioSource->setLooping( on );

	PlayerWidget * playerWidget = _playerWidgetForPlayerInfo( playerInfo );
	if ( playerWidget )
		playerWidget->updateLoop();
}


void MainWindow::on_menuWindow_aboutToShow()
{
	ui_.menuWindow->clear();

	{
		QAction * archiveTestAction = ui_.menuWindow->addAction( ArchiveTest::tr( "Archive Test" ),
			this, SLOT(_archiveTestTriggered()) );
		archiveTestAction->setCheckable( true );
		archiveTestAction->setChecked( archiveTest_ && archiveTest_->isVisible() );
	}

	if ( layoutMode_ == LayoutMode_Separate )
	{
		ui_.menuWindow->addSeparator();

		QAction * audioWindowAction = ui_.menuWindow->addAction( AudioWidget::tr( "Audio Settings" ),
			this, SLOT(_audioWindowTriggered()) );
		audioWindowAction->setCheckable( true );
		audioWindowAction->setChecked( audioWidget_->isVisible() );

		ui_.menuWindow->addSeparator();

		for ( QListIterator<PlayerInfo*> it ( playerInfos_ ); it.hasNext(); )
		{
			PlayerInfo * playerInfo = it.next();
			QAction * playerWindowAction = ui_.menuWindow->addAction( QFileInfo( playerInfo->filePath ).fileName(),
				this, SLOT(_playerWindowTriggered()) );
			playerWindowAction->setCheckable( true );
			playerWindowAction->setChecked( playerInfo->player && playerInfo->player->isVisible() );
			playerWindowAction->setData( QVariant::fromValue( playerInfo ) );
		}
	}
}


void MainWindow::_archiveTestTriggered()
{
	if ( !archiveTest_ )
		archiveTest_ = new ArchiveTest( this );

	archiveTest_->show();
	archiveTest_->activateWindow();
}


void MainWindow::_audioWindowTriggered()
{
	audioWidget_->show();
	audioWidget_->raise();
}


void MainWindow::_playerWindowTriggered()
{
	QAction * action = qobject_cast<QAction*>( sender() );
	Q_ASSERT( action );

	PlayerInfo * playerInfo = action->data().value<PlayerInfo*>();
	_showPlayer( playerInfo );
}


void MainWindow::on_menuSettings_aboutToShow()
{
	if ( languagesPopulated_ )
		return;

	languagesPopulated_ = true;

	LanguageManager * languageManager = LanguageManager::instance();

	QActionGroup * languagesActionGroup = new QActionGroup( this );

	for ( QStringListIterator it( languageManager->names() ); it.hasNext(); )
	{
		const QString & languageName = it.next();

		LanguageInfo languageInfo = languageManager->languageInfoForName( languageName );

		const QString actionText = QString::fromLatin1( "%1 (%2)" )
			.arg( languageInfo.nativeName ).arg( languageInfo.englishName );

		QAction * languageAction = ui_.menuSettings->addAction( actionText, this, SLOT(_languageTriggered()) );
		languageAction->setData( languageName );
		languageAction->setCheckable( true );
		languageAction->setChecked( languageName == languageManager->currentName() );

		languagesActionGroup->addAction( languageAction );
	}
}


void MainWindow::_languageTriggered()
{
	QAction * languageAction = qobject_cast<QAction*>( sender() );
	Q_ASSERT( languageAction );

	const QString languageName = languageAction->data().toString();

	LanguageManager::instance()->setCurrentName( languageName );
}


void MainWindow::on_actionAbout_triggered()
{
	QDialog aboutDialog( this );
	aboutDialog.setWindowFlags( Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint |
		Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint );
	Ui_AboutDialog ui;
	ui.setupUi( &aboutDialog );
	aboutDialog.setWindowTitle( QString( "%1 - %2" )
		.arg( tr( "About" ) )
		.arg( QCoreApplication::applicationName() ) );
	aboutDialog.exec();
}


void MainWindow::_pointActivated( const QModelIndex & index )
{
	const QString path = pointsModel_->pathForIndex( index );
	if ( path.isEmpty() )
		return;

	const QModelIndex browserIndex = browserModel_->QFileSystemModel::index( path, 0 );
	if ( !browserIndex.isValid() )
		return;

	// Bug in QTreeView or QFileSystemModel:
	// Scrolling may not occur until we will call this method two times.
	browserView_->scrollTo( browserIndex, QAbstractItemView::PositionAtTop );
	browserView_->setExpanded( browserIndex, true );
	browserView_->setCurrentIndex( browserIndex );

	{
		PlayerInfo * playerInfo = pointsModel_->playerInfoForIndex( index );
		if ( playerInfo )
			_showPlayer( playerInfo );
	}
}


void MainWindow::_browserActivated( const QModelIndex & index )
{
	if ( !index.isValid() )
		return;

	const QModelIndex firstIndex = browserModel_->firstIndex( index );

	{
		ArchiveInfo * archiveInfo = archiveInfoForArchiveIndex_.value( firstIndex );
		if ( archiveInfo )
		{
			_scrollToArchiveContents( archiveInfo );
			return;
		}
	}

	{
		PlayerInfo * playerInfo = playerInfoForIndex_.value( firstIndex );
		if ( playerInfo )
		{
			_showPlayer( playerInfo );
			return;
		}

		_populatePlayerMenu( QList<QModelIndex>() << firstIndex );
		if ( ui_.actionCreatePlayer->isEnabled() )
			ui_.actionCreatePlayer->trigger();
	}
}


void MainWindow::_archiveStateChanged()
{
	Grim::Archive * archive = qobject_cast<Grim::Archive*>( sender() );
	Q_ASSERT( archive );

	ArchiveInfo * archiveInfo = archiveInfoForArchive_.value( archive );
	Q_ASSERT( archiveInfo );

	if ( archiveInfo->archive->isBroken() )
	{
		// archive is broken, will not make attempt to open it more
		archiveInfoForArchive_.remove( archiveInfo->archive );
		delete archiveInfo->archive;
		archiveInfo->archive = 0;

		if ( archiveInfo->mountPointFile )
		{
			delete archiveInfo->mountPointFile;
			archiveInfo->mountPointFile = 0;
		}

		archiveInfoForMountPointIndex_.remove( archiveInfo->mountPointIndex );
		archiveInfo->mountPointIndex = QModelIndex();

		archiveInfoForMountPointPath_.remove( archiveInfo->mountPointPath );
		archiveInfo->mountPointPath = QString();

		archiveInfo->isBroken = true;
	}
	else
	{
		if ( archiveInfo->archive->state() == Grim::Archive::State_Ready )
		{
			// Archive is updated from source, this means that file operations
			// will not block and will take constant time.
			// For example user usually wants to open one directory deeper, if
			// archive contains only one root record.

			QFileInfoList topFiles = QDir( archiveInfo->archive->actualMountPoint() ).entryInfoList( QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot );
			if ( topFiles.count() == 1 && topFiles.first().isDir() )
			{
				const QString topFilePath = topFiles.first().absoluteFilePath();
				const QModelIndex topFileIndex = browserModel_->QFileSystemModel::index( topFilePath, 0 );
				browserView_->setExpanded( topFileIndex, true );
			}

			_scrollToArchiveContents( archiveInfo );
		}
	}

	_updateBrowserIndex( archiveInfo->archiveIndex );
}


void MainWindow::_sourceInitializationChanged( Grim::AudioSource * audioSource )
{
	PlayerInfo * playerInfo = playerInfoForSource_.value( audioSource );
	Q_ASSERT( playerInfo );

	PlayerWidget * playerWidget = _playerWidgetForPlayerInfo( playerInfo );
	if ( !playerWidget )
		return;

	playerWidget->sourceInitializationChanged();

	const QModelIndex firstIndex = playerInfo->index;
	const QModelIndex lastIndex = browserModel_->lastIndex( firstIndex );
	emit browserModel_->dataChanged( firstIndex, lastIndex );
}


void MainWindow::_sourceStateChanged( Grim::AudioSource * audioSource )
{
	PlayerInfo * playerInfo = playerInfoForSource_.value( audioSource );
	Q_ASSERT( playerInfo );

	PlayerWidget * playerWidget = _playerWidgetForPlayerInfo( playerInfo );
	if ( !playerWidget )
		return;

	playerWidget->sourceStateChanged();

	_updateBrowserIndex( playerInfo->index );
}


void MainWindow::_sourceCurrentOffsetChanged( Grim::AudioSource * audioSource )
{
	PlayerInfo * playerInfo = playerInfoForSource_.value( audioSource );
	Q_ASSERT( playerInfo );

	PlayerWidget * playerWidget = _playerWidgetForPlayerInfo( playerInfo );
	if ( !playerWidget )
		return;

	playerWidget->sourceCurrentOffsetChanged();
}


ArchiveInfo * MainWindow::mountArchive( const QModelIndex & index )
{
	const QFileInfo archiveFileInfo = browserModel_->fileInfo( index );
	if ( !archiveFileInfo.exists() || !archiveFileInfo.isFile() )
		return 0;

	const QString archiveFilePath = archiveFileInfo.absoluteFilePath();

	ArchiveInfo *& archiveInfo = archiveInfoForArchiveIndex_[ index ];
	if ( archiveInfo )
		return archiveInfo;

	archiveInfo = new ArchiveInfo;

	archiveInfo->isBroken = false;

	// create temporary file to use it as mount point
	QTemporaryFile * temporaryFile = new QTemporaryFile( QDir::temp().filePath( archiveFileInfo.fileName() ) );
	if ( !temporaryFile->open() )
	{
		// failed to open temporary file
		// so will use archive file path as mount point
		delete temporaryFile;
	}
	else
	{
		archiveInfo->mountPointFile = temporaryFile;
	}

	archiveInfo->archiveFilePath = archiveFilePath;
	archiveInfo->archiveIndex = index;

	archiveInfo->archive = new Grim::Archive( archiveFilePath );
	if ( archiveInfo->mountPointFile )
		archiveInfo->archive->setMountPoint( archiveInfo->mountPointFile->fileName() );

	archiveInfo->mountPointPath = archiveInfo->archive->actualMountPoint();
	archiveInfo->mountPointIndex = browserModel_->QFileSystemModel::index( archiveInfo->mountPointPath, 0 );

	connect( archiveInfo->archive, SIGNAL(stateChanged(int)), SLOT(_archiveStateChanged()) );

	archiveInfos_ << archiveInfo;
	archiveInfoForArchiveFilePath_[ archiveInfo->archiveFilePath ] = archiveInfo;
	archiveInfoForMountPointPath_[ archiveInfo->mountPointPath ] = archiveInfo;
	if ( archiveInfo->mountPointIndex.isValid() )
		archiveInfoForMountPointIndex_[ archiveInfo->mountPointIndex ] = archiveInfo;
	archiveInfoForArchive_[ archiveInfo->archive ] = archiveInfo;

	archiveInfo->archive->open( Grim::Archive::ReadOnly | Grim::Archive::DontLock );

	pointsModel_->addArchivePoint( archiveInfo );

	if ( archiveTest_ )
		archiveTest_->addDirPath( archiveInfo->mountPointPath );

	return archiveInfo;
}


void MainWindow::unmountArchive( ArchiveInfo * archiveInfo )
{
	if ( archiveTest_ )
		archiveTest_->removeDirPath( archiveInfo->mountPointPath );

	pointsModel_->removeArchivePoint( archiveInfo );

	archiveInfos_.removeOne( archiveInfo );
	archiveInfoForArchiveIndex_.remove( archiveInfo->archiveIndex );
	archiveInfoForArchiveFilePath_.remove( archiveInfo->archiveFilePath );
	archiveInfoForMountPointIndex_.remove( archiveInfo->mountPointIndex );
	archiveInfoForMountPointPath_.remove( archiveInfo->mountPointPath );
	archiveInfoForArchive_.remove( archiveInfo->archive );

	_updateBrowserIndex( archiveInfo->archiveIndex );

	_destroyArchiveInfo( archiveInfo );

	delete archiveInfo;
}


PlayerInfo * MainWindow::createPlayer( const QModelIndex & index )
{
	const QString filePath = browserModel_->filePath( index );
	if ( filePath.isEmpty() )
		return 0;

	PlayerInfo * playerInfo = new PlayerInfo;

	playerInfo->filePath = filePath;
	playerInfo->index = index;
	playerInfo->player = 0;

	Grim::AudioBuffer audioBuffer = audioWidget_->audioContext()->createBuffer(
		playerInfo->filePath, QByteArray(), Grim::AudioBuffer::Streaming );

	playerInfo->audioSource = audioWidget_->audioContext()->createSource();
	playerInfo->audioSource->setBuffer( audioBuffer );

	playerInfos_ << playerInfo;
	playerInfoForFilePath_[ playerInfo->filePath ] = playerInfo;
	playerInfoForIndex_[ playerInfo->index ] = playerInfo;
	playerInfoForSource_[ playerInfo->audioSource ] = playerInfo;

	pointsModel_->addPlayerPoint( playerInfo );

	return playerInfo;
}


void MainWindow::destroyPlayer( PlayerInfo * playerInfo )
{
	pointsModel_->removePlayerPoint( playerInfo );

	if ( singlePlayerInfo_ == playerInfo )
	{
		singlePlayerWidget_->setPlayerInfo( 0 );
		singlePlayerInfo_ = 0;
	}

	if ( playerInfo->audioSource )
		playerInfoForSource_.remove( playerInfo->audioSource );
	playerInfoForFilePath_.remove( playerInfo->filePath );
	playerInfoForIndex_.remove( playerInfo->index );
	playerInfos_.removeOne( playerInfo );

	_updateBrowserIndex( playerInfo->index );

	_destroyPlayerInfo( playerInfo );

	delete playerInfo;
}


void MainWindow::killAudioSources()
{
	singlePlayerWidget_->setPlayerInfo( 0 );

	for ( QListIterator<PlayerInfo*> it( playerInfos_ ); it.hasNext(); )
	{
		PlayerInfo * playerInfo = it.next();

		Q_ASSERT( playerInfo->audioSource );

		if ( playerInfo->player )
			playerInfo->player->playerWidget()->setPlayerInfo( 0 );

		playerInfoForSource_.remove( playerInfo->audioSource );

		delete playerInfo->audioSource;
		playerInfo->audioSource = 0;

		_updateBrowserIndex( playerInfo->index );
	}
}


void MainWindow::reviveAudioSources( const QList<PlayerInfo*> & playerInfos )
{
	for ( QListIterator<PlayerInfo*> it( playerInfos ); it.hasNext(); )
	{
		PlayerInfo * playerInfo = it.next();

		Q_ASSERT( !playerInfo->audioSource );

		Grim::AudioBuffer audioBuffer = audioWidget_->audioContext()->createBuffer(
			playerInfo->filePath, QByteArray(), Grim::AudioBuffer::Streaming );

		playerInfo->audioSource = audioWidget_->audioContext()->createSource();
		playerInfo->audioSource->setBuffer( audioBuffer );

		if ( playerInfo->player )
			playerInfo->player->playerWidget()->setPlayerInfo( playerInfo );

		playerInfoForSource_[ playerInfo->audioSource ] = playerInfo;

		_updateBrowserIndex( playerInfo->index );
	}

	if ( singlePlayerInfo_ && singlePlayerInfo_->audioSource )
		singlePlayerWidget_->setPlayerInfo( singlePlayerInfo_ );
}


QStringList MainWindow::mountPoints() const
{
	QStringList list;

	for ( QListIterator<ArchiveInfo*> it( archiveInfos_ ); it.hasNext(); )
	{
		const ArchiveInfo * archiveInfo = it.next();
		if ( !archiveInfo->archive )
			continue;
		list << archiveInfo->mountPointPath;
	}

	return list;
}


void MainWindow::audioContextCreated()
{
	Grim::AudioContext * audioContext = audioWidget_->audioContext();

	connect( audioContext, SIGNAL(sourceInitializationChanged(Grim::AudioSource*)),
		SLOT(_sourceInitializationChanged(Grim::AudioSource*)));
	connect( audioContext, SIGNAL(sourceStateChanged(Grim::AudioSource*)),
		SLOT(_sourceStateChanged(Grim::AudioSource*)));
	connect( audioContext, SIGNAL(sourceCurrentOffsetChanged(Grim::AudioSource*)),
		SLOT(_sourceCurrentOffsetChanged(Grim::AudioSource*)));

	reviveAudioSources( playerInfos_ );
}


void MainWindow::audioContextDestroyed()
{
	killAudioSources();
}


void MainWindow::retranslateUi()
{
	setWindowTitle( QCoreApplication::applicationName() );
}
