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

#include "utils.h"




namespace Grim {




/**
 * \namespace Grim
 *
 * \brief The Grim namespace wraps whole library.
 */




/**
 * \class Utils
 *
 * \ingroup tools_module
 *
 * \brief The Utils class is a namespace for useful utility functions.
 */


/**
 * Returns human readable string of \a bytes count with number of \a precision digits after dot.
 * If \a precision is 0 floating digits and dot character itself will be omitted.
 *
 * Returned string will be in format \<value\>\<letter\>,
 * where value is truncated most valuable part of \a bytes and letter represents measure.
 * Possible measures are:
 *
 * \li B - bytes
 * \li K - kilo
 * \li M - mega
 * \li G - giga
 * \li T - tera
 * \li P - peta
 * \li E - exa
 */

QString Utils::convertBytesToString( qint64 bytes, int precision )
{
	const bool isNegative = bytes < 0;
	const qint64 unsignedBytes = isNegative ? -bytes : bytes;

	int high;
	int low;

	int power;
	for ( power = 6; power > 0; --power )
	{
		high = (unsignedBytes >> power*10) & 0x03ff;
		if ( high )
		{
			low = (unsignedBytes >> (power-1)*10) & 0x03ff;
			break;
		}
	}

	if ( power == 0 )
	{
		high = unsignedBytes & 0x00001fff;
		low = 0;
	}

	QString letter =
		power == 0 ? tr( "B" ) : // bytes
		power == 1 ? tr( "K" ) : // kilo
		power == 2 ? tr( "M" ) : // mega
		power == 3 ? tr( "G" ) : // giga
		power == 4 ? tr( "T" ) : // tera
		power == 5 ? tr( "P" ) : // peta
		power == 6 ? tr( "E" ) : // exa
		power == 7 ? tr( "Z" ) : // zetta
		power == 8 ? tr( "Y" ) : // yotta
		QString();

	if ( precision > 0 )
	{
		Q_ASSERT( low >= 0 && low < 1024 );
		double mantissa = (double)((high << 10) | low)/1024;
		return isNegative ?
			QString( "-%1%3" ).arg( mantissa, 0, 'f', precision ).arg( letter ) :
			QString( "%1%3" ).arg( mantissa, 0, 'f', precision ).arg( letter );
	}
	else
	{
		return isNegative ?
			QString( "-%1%2" ).arg( high ).arg( letter ) :
			QString( "%1%2" ).arg( high ).arg( letter );
	}
}




} // namespace Grim
