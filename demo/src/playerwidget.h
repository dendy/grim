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

#include <QWidget>

#include "ui_playerwidget.h"




namespace Grim {
	class AudioSource;
}

class PlayerInfo;
class MainWindow;




class PlayerWidget : public QWidget
{
	Q_OBJECT

public:
	PlayerWidget( QWidget * parent, MainWindow * mainWindow );
	~PlayerWidget();

	PlayerInfo * playerInfo() const;
	void setPlayerInfo( PlayerInfo * playerInfo );

	void sourceInitializationChanged();
	void sourceStateChanged();
	void sourceCurrentOffsetChanged();

	void updateLoop();
	void updateGain();
	void updatePitch();

public slots:
	void on_buttonDestroy_clicked();
	void on_buttonPlay_clicked();
	void on_buttonPause_clicked();
	void on_buttonStop_clicked();
	void on_buttonLoop_toggled( bool on );
	void on_sliderGain_valueChanged( int value );
	void on_sliderPitch_valueChanged( int value );
	void on_sliderCurrentSampleOffset_valueChanged( int value );

protected:
	void changeEvent( QEvent * e );
	bool eventFilter( QObject * o, QEvent * e );

private:
	QString _pitchLabelText( float pitch );
	void _setSizes();
	void _setupSource();
	void _updateSourceInitialization();

	void _updateStateLabels();
	void _updateGainLabel();
	void _updatePitchLabel();
	void _updateTrackInfoLabels();
	void _updateCurrentSampleOffset();
	void _updateCurrentSampleOffsetLabels();

private:
	Ui_PlayerWidget ui_;

	MainWindow * mainWindow_;

	PlayerInfo * playerInfo_;
	Grim::AudioSource * audioSource_; // same as playerInfo_->audioSource, but shorter
};




inline PlayerInfo * PlayerWidget::playerInfo() const
{ return playerInfo_; }
