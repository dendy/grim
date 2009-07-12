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

#include "playerwidget.h"

#include <QTime>
#include <QContextMenuEvent>
#include <QFileInfo>

#include <grim/audio/audiobuffer.h>
#include <grim/audio/audiosource.h>

#include <math.h>

#include "mainwindow.h"
#include "resources.h"




static const QString TimeHourFormat    = QLatin1String( "H:mm:ss" );
static const QString TimeMinuteFormat  = QLatin1String( "m:ss" );
static const QString TimeSecondFormat  = QLatin1String( "s:zzz" );

static const float Log_E_2 = 0.693147180559945309417f;




static QTime _samplesToTime( qint64 samples, float frequency )
{
	Q_ASSERT( samples >= 0 && frequency > 0 );
	qint64 msecs = (qint64)( (float)samples*1000/frequency );
	return QTime().addMSecs( msecs );
}


static QString _minimalFormatForTime( const QTime & time )
{
	if ( time.hour() > 0 )
		return TimeHourFormat;
	if ( time.minute() > 0 )
		return TimeMinuteFormat;
	return TimeSecondFormat;
}


static int _sourcePitchToSliderPitch( float pitch )
{
	const float pitchPower = log( pitch ) / Log_E_2;
	const int pitchValue = qBound<int>( -10000, (int)(pitchPower*1000), 10000 );
	return pitchValue;
}


static float _sliderPitchToSourcePitch( int pitchValue )
{
	const float pitchPower = (float)pitchValue/1000;
	const float pitch = exp( Log_E_2 * pitchPower );
	return pitch;
}




PlayerWidget::PlayerWidget( QWidget * parent, MainWindow * mainWindow ) :
	QWidget( parent ),
	mainWindow_( mainWindow )
{
	ui_.setupUi( this );

	// for popup menu "Reset pitch"
	ui_.sliderPitch->installEventFilter( this );

	_setSizes();

	playerInfo_ = 0;
	audioSource_ = 0;

	_setupSource();
}


PlayerWidget::~PlayerWidget()
{
}


void PlayerWidget::setPlayerInfo( PlayerInfo * playerInfo )
{
	if ( playerInfo_ == playerInfo )
		return;

	if ( audioSource_ )
		audioSource_->setSignalsBlocked( true );

	playerInfo_ = playerInfo;
	audioSource_ = playerInfo_ ? playerInfo_->audioSource : 0;

	if ( audioSource_ )
		audioSource_->setSignalsBlocked( false );

	_setupSource();
}


void PlayerWidget::sourceInitializationChanged()
{
	_updateSourceInitialization();
}


void PlayerWidget::sourceStateChanged()
{
	_updateStateLabels();
}


void PlayerWidget::sourceCurrentOffsetChanged()
{
	_updateCurrentSampleOffset();
}


void PlayerWidget::updateLoop()
{
	ui_.buttonLoop->blockSignals( true );
	ui_.buttonLoop->setChecked( audioSource_ ? audioSource_->isLooping() : false );
	ui_.buttonLoop->blockSignals( false );
}


void PlayerWidget::updateGain()
{
	ui_.sliderGain->blockSignals( true );
	ui_.sliderGain->setValue( audioSource_ ? audioSource_->gain()*1000 : 0.5f );
	ui_.sliderGain->blockSignals( false );

	_updateGainLabel();
}


void PlayerWidget::updatePitch()
{
	const int pitchValue = audioSource_ ? _sourcePitchToSliderPitch( audioSource_->pitch() ) : 0;
	ui_.sliderPitch->blockSignals( true );
	ui_.sliderPitch->setValue( pitchValue );
	ui_.sliderPitch->blockSignals( false );

	_updatePitchLabel();
}


void PlayerWidget::on_buttonDestroy_clicked()
{
	mainWindow_->destroyPlayer( playerInfo_ );
}


void PlayerWidget::on_buttonPlay_clicked()
{
	audioSource_->play();
}


void PlayerWidget::on_buttonPause_clicked()
{
	audioSource_->pause();
}


void PlayerWidget::on_buttonStop_clicked()
{
	audioSource_->stop();
}


void PlayerWidget::on_buttonLoop_toggled( bool on )
{
	audioSource_->setLooping( on );
}


void PlayerWidget::on_sliderGain_valueChanged( int value )
{
	audioSource_->setGain( (float)value/1000 );
	_updateGainLabel();
}


void PlayerWidget::on_sliderPitch_valueChanged( int value )
{
	const float pitch = _sliderPitchToSourcePitch( value );

	audioSource_->setPitch( pitch );

	_updatePitchLabel();
}


void PlayerWidget::on_sliderCurrentSampleOffset_valueChanged( int value )
{
	const bool savedAreSignalsBlocked = audioSource_->setSignalsBlocked( true );
	audioSource_->setCurrentSampleOffset( value );
	audioSource_->setSignalsBlocked( savedAreSignalsBlocked );

	_updateCurrentSampleOffsetLabels();
}


void PlayerWidget::changeEvent( QEvent * e )
{
	if ( e->type() == QEvent::LanguageChange )
	{
		ui_.retranslateUi( this );

		_setSizes();

		if ( audioSource_ )
		{
			_updateStateLabels();
			_updateGainLabel();
			_updatePitchLabel();
			_updateTrackInfoLabels();
			_updateCurrentSampleOffsetLabels();
		}
	}

	QWidget::changeEvent( e );
}


bool PlayerWidget::eventFilter( QObject * o, QEvent * e )
{
	if ( o == ui_.sliderPitch && e->type() == QEvent::ContextMenu )
	{
		QContextMenuEvent * ce = static_cast<QContextMenuEvent*>( e );

		if ( !audioSource_ )
			return true;

		QMenu menu;
		QAction * resetPitchAction = menu.addAction( tr( "Reset pitch" ) );
		QAction * triggeredAction = menu.exec( ce->globalPos() );
		if ( triggeredAction == resetPitchAction )
			ui_.sliderPitch->setValue( 0 );

		return true;
	}

	return QWidget::eventFilter( o, e );
}


QString PlayerWidget::_pitchLabelText( float pitch )
{
	QString text = QString::number( pitch, 'f', 4 );
	text.truncate( 6 );
	return text;
}


void PlayerWidget::_setSizes()
{
	// sizes for static labels: channels, bits per sample, frequency
	{
		QWidgetList widgets;
		widgets <<
			ui_.labelChannelsStatic <<
			ui_.labelBitsPerSampleStatic <<
			ui_.labelFrequencyStatic;

		int minimumWidth = 0;
		QListIterator<QWidget*> it( widgets );
		for ( ; it.hasNext(); )
			minimumWidth = qMax( minimumWidth, it.next()->sizeHint().width() );
		for ( it.toFront(); it.hasNext(); )
			it.next()->setMinimumWidth( minimumWidth );
	}

	// sizes for value labels: channels, bits per sample, frequency
	{
		QWidgetList widgets;
		widgets <<
			ui_.labelChannels <<
			ui_.labelBitsPerSample <<
			ui_.labelFrequency;

		int minimumWidth = QFontMetrics( ui_.labelChannels->font() ).width( QLatin1String( "00000000" ) );
		for ( QListIterator<QWidget*> it( widgets ); it.hasNext(); )
			it.next()->setMinimumWidth( minimumWidth );
	}

	// size for times label
	{
		QFont font =
#ifdef Q_WS_X11
			QFont( "Monospace" );
#elif defined Q_WS_WIN
			QFont( "Courier" );
#else
			QFont( "Arial" );
#endif

		ui_.labelTimeCurrent->setFont( font );
		ui_.labelTimeTotal->setFont( font );
	}

	// size for pitch value label
	{
		QFontMetrics fontMetrics( ui_.labelPitch->font() );
		const QString minPitchText = _pitchLabelText( _sliderPitchToSourcePitch( -10000 ) );
		const QString maxPitchText = _pitchLabelText( _sliderPitchToSourcePitch( +10000 ) );
		const int minimumWidth = qMax( fontMetrics.width( minPitchText ), fontMetrics.width( maxPitchText ) );

		ui_.labelPitch->setFixedWidth( minimumWidth );
	}
}


void PlayerWidget::_setupSource()
{
	setUpdatesEnabled( false );

	const bool hasAudioSource = audioSource_ != 0;

	setEnabled( hasAudioSource );

	// inititialized-invariant values
	ui_.labelFileName->setText( hasAudioSource ? QFileInfo( playerInfo_->filePath ).fileName() : QString() );
	ui_.buttonDestroy->setEnabled( hasAudioSource );

	_updateStateLabels();

	updateLoop();
	updateGain();
	updatePitch();

	// initialized-depended values
	_updateSourceInitialization();

	setUpdatesEnabled( true );
}


void PlayerWidget::_updateSourceInitialization()
{
	if ( audioSource_ )
	{
		// enable/disable initialization-dependent controls
		ui_.sliderCurrentSampleOffset->setEnabled( audioSource_->isInitialized() );
	}

	if ( !audioSource_ || !audioSource_->isInitialized() )
	{
		QFontMetrics fontMetrics( ui_.labelTimeCurrent->font() );
		const QString longestTimeString = QTime( 11, 11, 11 ).toString( TimeHourFormat );
		const int minimumSize = fontMetrics.width( longestTimeString );

		ui_.labelTimeCurrent->setFixedWidth( minimumSize );
		ui_.labelTimeTotal->setFixedWidth( minimumSize );

		ui_.labelTimeCurrent->setText( QTime( 0, 0, 0 ).toString( TimeHourFormat ) );
		ui_.labelTimeTotal->setText( QTime( 0, 0, 0 ).toString( TimeHourFormat ) );
	}
	else
	{
		const qint64 totalSamples = playerInfo_->audioSource->totalSamples();
		const float frequency = playerInfo_->audioSource->frequency();

		const QTime totalTime = totalSamples == -1 ? QTime( 11, 11, 11 ) : _samplesToTime( totalSamples, frequency );
		const QString format = _minimalFormatForTime( totalTime );

		QFontMetrics fontMetrics( ui_.labelTimeCurrent->font() );
		const QString longestTimeString = totalTime.toString( format );
		const int minimumSize = fontMetrics.width( longestTimeString );

		ui_.labelTimeCurrent->setFixedWidth( minimumSize );
		ui_.labelTimeTotal->setFixedWidth( minimumSize );

		ui_.sliderCurrentSampleOffset->setEnabled( !playerInfo_->audioSource->isSequential() );
	}

	_updateTrackInfoLabels();
	_updateCurrentSampleOffset();
}


void PlayerWidget::_updateStateLabels()
{
	QString stateText;
	QIcon stateIcon;

	if ( audioSource_ )
	{
		switch ( audioSource_->state() )
		{
		case Grim::AudioSource::State_Idle:
			stateText = tr( "Idle" );
			break;
		case Grim::AudioSource::State_Stopped:
			stateIcon = QIcon( IconPlaybackStopPath );
			break;
		case Grim::AudioSource::State_Playing:
			stateIcon = QIcon( IconPlaybackPlayPath );
			break;
		case Grim::AudioSource::State_Paused:
			stateIcon = QIcon( IconPlaybackPausePath );
			break;
		}
	}

	ui_.labelState->setText( stateText );
	ui_.labelState->setPixmap( stateIcon.pixmap( ui_.labelState->size() ) );
}


void PlayerWidget::_updateGainLabel()
{
	const float gain = audioSource_ ? audioSource_->gain() : 0.5f;
	ui_.labelGain->setText( QString::number( gain, 'f', 2 ) );
}


void PlayerWidget::_updatePitchLabel()
{
	const float pitch = audioSource_ ? audioSource_->pitch() : 1.0f;
	ui_.labelPitch->setText( _pitchLabelText( pitch ) );
}


void PlayerWidget::_updateTrackInfoLabels()
{
	QString channelsText;
	QString bitsPerSampleText;
	QString frequencyText;

	if( audioSource_  && audioSource_->isInitialized() )
	{
		channelsText = audioSource_->channelsCount() == 1 ? tr( "Mono" ) : tr( "Stereo" );
		bitsPerSampleText = QString::number( audioSource_->bitsPerSample() );
		frequencyText = tr( "%1 Hz" ).arg( audioSource_->frequency() );
	}

	ui_.labelChannels->setText( channelsText );
	ui_.labelBitsPerSample->setText( bitsPerSampleText );
	ui_.labelFrequency->setText( frequencyText );
}


void PlayerWidget::_updateCurrentSampleOffset()
{
	qint64 currentSampleOffset;
	qint64 totalSamples;

	if ( !audioSource_ || !audioSource_->isInitialized() )
	{
		currentSampleOffset = 0;
		totalSamples = 0;
	}
	else
	{
		currentSampleOffset = audioSource_->currentSampleOffset();
		totalSamples = audioSource_->totalSamples();
	}

	ui_.sliderCurrentSampleOffset->blockSignals( true );
	ui_.sliderCurrentSampleOffset->setRange( 0, totalSamples == -1 ? currentSampleOffset*2 : totalSamples );
	ui_.sliderCurrentSampleOffset->setValue( currentSampleOffset );
	ui_.sliderCurrentSampleOffset->blockSignals( false );

	_updateCurrentSampleOffsetLabels();
}


void PlayerWidget::_updateCurrentSampleOffsetLabels()
{
	if ( !audioSource_ || !audioSource_->isInitialized() )
	{
	}
	else
	{
		const qint64 currentSampleOffset = audioSource_->currentSampleOffset();
		const qint64 totalSamples = audioSource_->totalSamples();
		const float frequency = audioSource_->frequency();

		const QTime currentTime = _samplesToTime( currentSampleOffset, frequency );
		const QTime totalTime = totalSamples == -1 ? QTime( 11, 11, 11 ) : _samplesToTime( totalSamples, frequency );

		const QString format = _minimalFormatForTime( totalTime );
		ui_.labelTimeCurrent->setText( currentTime.toString( format ) );
		ui_.labelTimeTotal->setText( totalSamples == -1 ? tr( "?" ) : totalTime.toString( format ) );
	}
}
