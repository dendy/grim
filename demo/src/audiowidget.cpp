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

#include "audiowidget.h"

#include <QStandardItemModel>
#include <QMessageBox>

#include <grim/audio/audiomanager.h>
#include <grim/audio/audiodevice.h>
#include <grim/audio/audiocontext.h>

#include "mainwindow.h"




AudioWidget::AudioWidget( MainWindow * mainWindow ) :
	mainWindow_( mainWindow )
{
	ui_.setupUi( this );

	audioDevice_ = 0;
	audioContext_ = 0;

	devicesModel_ = new QStandardItemModel( this );

	// populate devices list
	Grim::AudioManager * audioManager = Grim::AudioManager::sharedManager();
	const QByteArray defaultDeviceName = audioManager->defaultDeviceName();
	for ( QListIterator<QByteArray> devicesIt( audioManager->availableDeviceNames() ); devicesIt.hasNext(); )
	{
		const QByteArray & deviceName = devicesIt.next();

		QStandardItem * item = new QStandardItem( QString::fromUtf8( deviceName ) );
		item->setTextAlignment( Qt::AlignCenter );
		item->setEditable( false );
		item->setData( deviceName );

		if ( deviceName == defaultDeviceName )
		{
			QFont font = item->font();
			font.setItalic( true );
			item->setFont( font );
		}

		devicesModel_->appendRow( item );
	}

	ui_.viewDevices->setModel( devicesModel_ );

	connect( ui_.viewDevices->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(_updateButtons()) );

#if 0
	for ( QListIterator<QByteArray> fileFormatIt( audioManager->availableFileFormats() ); fileFormatIt.hasNext(); )
	{
		const QByteArray & fileFormat = fileFormatIt.next();
		qDebug() << "Dialog::initAudio() : File format available:" << fileFormat;
	}
#endif

	_updateButtons();

	retranslateUi();
}


AudioWidget::~AudioWidget()
{
	if ( hasAudio() )
		_voidSelf();
}


bool AudioWidget::hasAudio() const
{
	return audioContext_ != 0;
}


void AudioWidget::initializeAudio()
{
	static const QByteArray AlsaSoftwareDeviceName = "ALSA Software";

	Grim::AudioManager * audioManager = Grim::AudioManager::sharedManager();
	const QByteArray deviceName = audioManager->availableDeviceNames().contains( AlsaSoftwareDeviceName ) ?
		AlsaSoftwareDeviceName : audioManager->defaultDeviceName();

	if ( deviceName.isEmpty() )
		return;

	_createSelf( deviceName, true );

	_updateButtons();
	_updateCurrentDevice();
}


void AudioWidget::retranslateUi()
{
	setWindowTitle( QString( "%1 - %2" )
		.arg( tr( "Audio Settings" ) )
		.arg( QCoreApplication::applicationName() ) );
}


void AudioWidget::on_buttonSwitch_clicked()
{
	const QModelIndex index = ui_.viewDevices->currentIndex();
	Q_ASSERT( index.isValid() );

	if ( audioDevice_ )
		_voidSelf();

	const QByteArray deviceName = devicesModel_->itemFromIndex( index )->data().value<QByteArray>();

	_createSelf( deviceName, true );

	_updateButtons();
	_updateCurrentDevice();
}


void AudioWidget::on_buttonVoid_clicked()
{
	Q_ASSERT( audioDevice_ );

	_voidSelf();

	_updateButtons();
	_updateCurrentDevice();
}


void AudioWidget::changeEvent( QEvent * e )
{
	if ( e->type() == QEvent::LanguageChange )
	{
		ui_.retranslateUi( this );
		retranslateUi();
	}

	QWidget::changeEvent( e );
}


void AudioWidget::_updateButtons()
{
	ui_.buttonSwitch->setEnabled( ui_.viewDevices->currentIndex().isValid() );
	ui_.buttonVoid->setEnabled( audioDevice_ != 0 );
}


void AudioWidget::_updateCurrentDevice()
{
	const int rowCount = devicesModel_->rowCount();

	for ( int row = 0; row < rowCount; ++row )
	{
		QStandardItem * item = devicesModel_->item( row );

		const QByteArray deviceName = item->data().value<QByteArray>();

		QFont font = item->font();
		font.setBold( deviceName == currentAudioDeviceName_ );
		item->setFont( font );
	}
}


void AudioWidget::_createSelf( const QByteArray & deviceName, bool showWarnings )
{
	audioDevice_ = Grim::AudioManager::sharedManager()->createDevice( deviceName );
	if ( !audioDevice_ )
	{
		if ( showWarnings )
			QMessageBox::information( this, mainWindow_->windowTitle(),
				tr( "Audio device (<b>%1</b>) failed to initialize.<br>"
					"Please select different audio device and try again." ).arg( QLatin1String( deviceName ) ) );
		return;
	}

	audioContext_ = audioDevice_->createContext();
	if ( !audioContext_ )
	{
		if ( showWarnings )
			QMessageBox::information( this, mainWindow_->windowTitle(),
				tr( "Audio context failed to initialize." ) );

		delete audioDevice_;
		audioDevice_ = 0;
	}

	currentAudioDeviceName_ = deviceName;

	mainWindow_->audioContextCreated();
}


void AudioWidget::_voidSelf()
{
	Q_ASSERT( audioDevice_ );

	mainWindow_->audioContextDestroyed();

	delete audioContext_;
	audioContext_ = 0;

	delete audioDevice_;
	audioDevice_ = 0;

	currentAudioDeviceName_ = QByteArray();
}
