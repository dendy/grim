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

#include "audioglobal.h"




namespace Grim {




class GRIM_AUDIO_EXPORT AudioVector
{
public:
	AudioVector();
	AudioVector( const AudioVector & vector );
	AudioVector( float x, float y, float z );
	AudioVector & operator=( const AudioVector & vector );
	bool operator==( const AudioVector & vector ) const;

	bool isNull() const;

	float x() const;
	float & rx();

	float y() const;
	float & ry();

	float z() const;
	float & rz();

	const float * constData() const;
	float * data();

private:
	float xyz_[ 3 ];
};




inline AudioVector::AudioVector()
{ rx() = ry() = rz() = 0.0f; }

inline AudioVector::AudioVector( const AudioVector & vector )
{ rx() = vector.x(); ry() = vector.y(); rz() = vector.z(); }

inline AudioVector::AudioVector( float x, float y, float z )
{ rx() = x; ry() = y; rz() = z; }

inline AudioVector & AudioVector::operator=( const AudioVector & vector )
{ rx() = vector.x(); ry() = vector.y(); rz() = vector.z(); return *this; }

inline bool AudioVector::operator==( const AudioVector & vector ) const
{ return x() == vector.x() && y() == vector.y() && z() == vector.z(); }

inline bool AudioVector::isNull() const
{ return x() == 0 && y() == 0 && z() == 0; }

inline float AudioVector::x() const
{ return xyz_[ 0 ]; }

inline float & AudioVector::rx()
{ return xyz_[ 0 ]; }

inline float AudioVector::y() const
{ return xyz_[ 1 ]; }

inline float & AudioVector::ry()
{ return xyz_[ 1 ]; }

inline float AudioVector::z() const
{ return xyz_[ 2 ]; }

inline float & AudioVector::rz()
{ return xyz_[ 2 ]; }

inline const float * AudioVector::constData() const
{ return xyz_; }

inline float * AudioVector::data()
{ return xyz_; }




} // namespace Grim
