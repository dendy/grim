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

#include <QSharedDataPointer>




namespace Grim {




class IdGeneratorPrivate;
class IdGeneratorIteratorPrivate;




class GRIM_TOOLS_EXPORT IdGenerator
{
public:
	IdGenerator( int limit = -1 );
	IdGenerator( const IdGenerator & generator );
	IdGenerator & operator=( const IdGenerator & generator );
	~IdGenerator();

	int limit() const;

	int take();
	void free( int id );
	void reserve( int id );
	bool isFree( int id ) const;

	bool isEmpty() const;
	int count() const;

private:
	QSharedDataPointer<IdGeneratorPrivate> d_;

	friend class IdGeneratorIterator;
};




class GRIM_TOOLS_EXPORT IdGeneratorIterator
{
public:
	IdGeneratorIterator( const IdGenerator & idGenerator );
	~IdGeneratorIterator();

	bool hasNext() const;
	int next();

private:
	IdGeneratorIteratorPrivate * d_;
};




} // namespace Grim
