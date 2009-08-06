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

#include <QDataStream>




namespace Grim {




class GRIM_TOOLS_EXPORT DataStreamReader
{
public:
	DataStreamReader( QDataStream & ds );

	qint8 readInt8();
	quint8 readUint8();

	qint16 readInt16();
	quint16 readUint16();

	qint32 readInt32();
	quint32 readUint32();

	qint64 readInt64();
	quint64 readUint64();

private:
	QDataStream & ds_;
};




inline DataStreamReader::DataStreamReader( QDataStream & ds ) :
	ds_( ds )
{}

inline qint8 DataStreamReader::readInt8()
{ qint8 i; ds_ >> i; return i; }

inline quint8 DataStreamReader::readUint8()
{ quint8 i; ds_ >> i; return i; }

inline qint16 DataStreamReader::readInt16()
{ qint16 i; ds_ >> i; return i; }

inline quint16 DataStreamReader::readUint16()
{ quint16 i; ds_ >> i; return i; }

inline qint32 DataStreamReader::readInt32()
{ qint32 i; ds_ >> i; return i; }

inline quint32 DataStreamReader::readUint32()
{ quint32 i; ds_ >> i; return i; }

inline qint64 DataStreamReader::readInt64()
{ qint64 i; ds_ >> i; return i; }

inline quint64 DataStreamReader::readUint64()
{ quint64 i; ds_ >> i; return i; }




} // namespace Grim
