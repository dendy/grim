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

#include "player.h"

#include <QFileInfo>
#include <QCloseEvent>

#include <grim/audio/audiosource.h>

#include "mainwindow.h"
#include "playerwidget.h"




Player::Player( PlayerInfo * playerInfo, MainWindow * mainWindow ) :
	QWidget( mainWindow, Qt::Dialog ),
	mainWindow_( mainWindow )
{
	playerWidget_ = new PlayerWidget( 0, mainWindow_ );
	playerWidget_->setPlayerInfo( playerInfo );

	QVBoxLayout * layout = new QVBoxLayout( this );
	layout->addWidget( playerWidget_ );

	retranslateUi();
}


Player::~Player()
{
}


void Player::retranslateUi()
{
	setWindowTitle( QString( "%1 - %2" )
		.arg( QFileInfo( playerWidget_->playerInfo()->filePath ).fileName() )
		.arg( QCoreApplication::applicationName() ) );
}


void Player::changeEvent( QEvent * e )
{
	if ( e->type() == QEvent::LanguageChange )
		retranslateUi();

	QWidget::changeEvent( e );
}


void Player::closeEvent( QCloseEvent * e )
{
	Q_ASSERT( playerWidget_->playerInfo() );

	if ( playerWidget_->playerInfo()->audioSource )
		playerWidget_->playerInfo()->audioSource->pause();

	e->accept();
}
