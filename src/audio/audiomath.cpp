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

#include "audiomath.h"




namespace Grim {




/**
 * \class AudioVector
 *
 * \ingroup audio_module
 *
 * \brief The AudioVector class represents vector in 3D space.
 *
 * \reentrant
 *
 * Default constructor creates zero vector, which also can be tested with isNull().
 */


/**
 * \fn AudioVector::AudioVector()
 *
 * Constructs zero vector.
 */


/**
 * \fn AudioVector::AudioVector( const AudioVector & vector )
 *
 * Constructs copy of the given \a vector.
 */


/**
 * \fn AudioVector::AudioVector( float x, float y, float z )
 *
 * Constructs vector from the given coordinates \a x, \a y and \a z.
 */


/**
 * \fn AudioVector & AudioVector::operator=( const AudioVector & vector )
 *
 * Equals this vector to the given \a vector and returns self.
 */


/**
 * \fn bool AudioVector::operator==( const AudioVector & vector ) const
 *
 * Returns \c true if all coordinates of this vector are equals to the given \a vector.
 * Otherwise returns \c false.
 */


/**
 * \fn bool AudioVector::isNull() const
 *
 * Returns whether this vector is zero.
 */


/**
 * \fn float AudioVector::x() const
 *
 * Returns x coordinate.
 */


/**
 * \fn float & AudioVector::rx()
 *
 * Returns reference to x coordinate for mutable operations.
 */


/**
 * \fn float AudioVector::y() const
 *
 * Returns y coordinate.
 */


/**
 * \fn float & AudioVector::ry()
 *
 * Returns reference to y coordinate for mutable operations.
 */


/**
 * \fn float AudioVector::z() const
 *
 * Returns z coordinate.
 */


/**
 * \fn float & AudioVector::rz()
 *
 * Returns reference to z coordinate for mutable operations.
 */


/**
 * \fn const float * AudioVector::constData() const
 *
 * Returns pointer to 3 coordinates, x, y and z.
 */


/**
 * \fn float * AudioVector::data()
 *
 * Returns pointer mutable 3 coordinates, x, y and z.
 */




} // namespace Grim
