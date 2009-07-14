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

#include <QObject>
#include <QTime>




namespace Grim {




class FpsCounterPrivate;




class GRIM_TOOLS_EXPORT FpsCounter : public QObject
{
	Q_OBJECT

	Q_PROPERTY( float value READ value )
	Q_PROPERTY( int timeout READ timeout WRITE setTimeout )

public:
	FpsCounter( QObject * parent = 0 );
	~FpsCounter();

	float value() const;

	int timeout() const;
	void setTimeout( int timeout );

	void reset();

	float hit( int msecs = -1 );

signals:
	void valueChanged( float value );

private:
	FpsCounterPrivate * d_;
};




} // namespace Grim
