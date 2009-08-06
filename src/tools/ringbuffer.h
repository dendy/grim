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

#include "toolsglobal.h"

#include <QIODevice>




namespace Grim {




class RingBufferPrivate;




class GRIM_TOOLS_EXPORT RingBuffer : public QIODevice
{
	Q_OBJECT

public:
	RingBuffer( QObject * parent = 0 );
	~RingBuffer();

	void push( const char * data, int size );
	void push( const QByteArray & data );
	void pop( int size );
	void clear();

	// reimplemented from QIODevice
	bool open( QIODevice::OpenMode openMode );

	bool isSequential() const;
	qint64 bytesAvailable() const;
	qint64 size() const;

	bool seek( qint64 pos );

protected:
	qint64 readData( char * data, qint64 maxLen );
	qint64 writeData( const char * data, qint64 maxLen );

private:
	RingBufferPrivate * d_;

	friend class RingBufferPrivate;
};




} // namespace Grim
