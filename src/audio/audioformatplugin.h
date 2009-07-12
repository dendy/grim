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

#pragma once

#include <QObject>
#include <QtPlugin>




class QIODevice;




namespace Grim {




class AudioFormatFilePrivate;




class AudioFormatFile
{
public:
	AudioFormatFile( const QString & fileName, const QByteArray & format );
	virtual ~AudioFormatFile();

	virtual QIODevice * device() = 0;

	QString fileName() const;
	QByteArray format() const;
	QByteArray resolvedFormat() const;

	int channels() const;
	int frequency() const;
	int bitsPerSample() const;
	qint64 totalSamples() const;

	qint64 truncatedSize( qint64 size ) const;

	qint64 bytesToSamples( qint64 bytes ) const;
	qint64 samplesToBytes( qint64 samples ) const;

protected:
	void setResolvedFormat( const QByteArray & format );
	void setChannels( int channels );
	void setFrequency( int frequency );
	void setBitsPerSample( int bitsPerSample );
	void setTotalSamples( qint64 totalSamples );

private:
	AudioFormatFilePrivate * d_;
};





class AudioFormatPlugin
{
public:
	virtual ~AudioFormatPlugin() {}

	virtual QList<QByteArray> formats() const = 0;
	virtual QStringList extensions() const = 0;

	virtual AudioFormatFile * createFile( const QString & fileName, const QByteArray & format = QByteArray() ) = 0;
};




} // namespace Grim




Q_DECLARE_INTERFACE( Grim::AudioFormatPlugin, "ua.org.dendy/Grim/AudioFormatPlugin/1.0" )
