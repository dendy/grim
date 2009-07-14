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

#include "ui_audiowidget.h"




class QStandardItemModel;

namespace Grim {
	class AudioDevice;
	class AudioContext;
}

class MainWindow;




class AudioWidget : public QWidget
{
	Q_OBJECT

public:
	AudioWidget( MainWindow * mainWindow );
	~AudioWidget();

	bool hasAudio() const;

	Grim::AudioContext * audioContext() const;

	void initializeAudio();

	void retranslateUi();

public slots:
	void on_buttonSwitch_clicked();
	void on_buttonVoid_clicked();

protected:
	void changeEvent( QEvent * e );

private slots:
	void _updateButtons();
	void _updateCurrentDevice();

private:
	void _createSelf( const QByteArray & deviceName, bool showWarnings );
	void _voidSelf();

private:
	Ui_AudioWidget ui_;

	MainWindow * mainWindow_;

	Grim::AudioDevice * audioDevice_;
	Grim::AudioContext * audioContext_;
	QByteArray currentAudioDeviceName_;

	QStandardItemModel * devicesModel_;
};




inline Grim::AudioContext * AudioWidget::audioContext() const
{ return audioContext_; }
