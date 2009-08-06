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

#include "datastreamreader.h"




namespace Grim {




/**
 * \class DataStreamReader
 *
 * \ingroup tools_module
 *
 * \brief The DataStreamReader class adds convenient methods to read integer values from QDataStream.
 *
 * QDataStream has universal interface to read/write variables of concrete types: operator<<() and operator>>().
 * Sometimes internal variable type in memory distinct from one that was serialized.
 *
 * To write such variable to stream it usually wrapped in a way like:
 *
 * \code
 * int myValue;
 * ...
 * QDataStream stream;
 * stream << qint16( myValue );
 * \endcode
 *
 * On read side this serialized type must be mirrored:
 *
 * \code
 * QDataStream stream;
 * qint16 tmpMyInt;
 * stream >> tmpMyInt;
 * myInt = tmpMyInt;
 * \endcode
 *
 * The DataStreamReaded class allows to avoid creation of temporary variable:
 *
 * \code
 * QDataStream stream;
 * DataStreamReaded streamReader( stream );
 * myInt = streamReader.readInt16();
 * \endcode
 */


/**
 * \fn DataStreamReader::DataStreamReader( QDataStream & stream )
 *
 * Constructs reader that will read integers from the given \a stream.
 */


/**
 * \fn qint8 DataStreamReader::readInt8()
 *
 * Reads \c qint8 value from stream and returns it.
 */


/**
 * \fn quint8 DataStreamReader::readUint8()
 *
 * Reads \c quint8 value from stream and returns it.
 */


/**
 * \fn qint16 DataStreamReader::readInt16()
 *
 * Reads \c qint16 value from stream and returns it.
 */


/**
 * \fn quint16 DataStreamReader::readUint16()
 *
 * Reads \c quint16 value from stream and returns it.
 */


/**
 * \fn qint32 DataStreamReader::readInt32()
 *
 * Reads \c qint32 value from stream and returns it.
 */


/**
 * \fn quint32 DataStreamReader::readUint32()
 *
 * Reads \c quint32 value from stream and returns it.
 */


/**
 * \fn qint64 DataStreamReader::readInt64()
 *
 * Reads \c qint64 value from stream and returns it.
 */


/**
 * \fn quint64 DataStreamReader::readUint64()
 *
 * Reads \c quint64 value from stream and returns it.
 */




} // namespace Grim
