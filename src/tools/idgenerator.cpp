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

#include "idgenerator.h"

#include <QVector>




namespace Grim {




static const int DefaultEnlargeSize = 1024;




class IdGeneratorPrivate : public QSharedData
{
public:
	IdGeneratorPrivate( int limit );

	int take();
	void free( int id );
	void reserve( int id );
	bool isFree( int id ) const;

private:
	void _init( int limit );
	struct Id;
	void _makeIdTaken( int id, Id * idp );

private:
	struct Id
	{
		int next;
		int previous;
		bool isFree;
	};

	int limit_;
	int first_;
	int last_;
	int firstTaken_;
	int lastTaken_;
	int freeCount_;
	QVector<Id> ids_;

	friend class IdGenerator;
	friend class IdGeneratorIterator;
	friend class IdGeneratorIteratorPrivate;
};




/** \internal
 * Initializes a generator with the given \a size of table of identifiers.
 */

inline IdGeneratorPrivate::IdGeneratorPrivate( int limit ) :
	limit_( limit )
{
	Q_ASSERT( limit_ >= -1 );

	freeCount_ = 0;
	first_ = 0;
	last_ = 0;

	firstTaken_ = 0;
	lastTaken_ = 0;

	ids_.reserve( limit == -1 ? DefaultEnlargeSize : qMin( limit + 1, DefaultEnlargeSize ) );
	ids_.resize( 1 );
	ids_[ 0 ].previous = 0;
	ids_[ 0 ].next = 0;
}


inline void IdGeneratorPrivate::_makeIdTaken( int id, Id * idp )
{
	Id & currentId = idp[ id ];

	currentId.isFree = false;

	currentId.previous = lastTaken_;
	if ( lastTaken_ != 0 )
	{
		idp[ lastTaken_ ].next = id;
	}
	else
	{
		firstTaken_ = lastTaken_ = id;
	}
	currentId.next = 0;
}


/** \internal
 */

inline int IdGeneratorPrivate::take()
{
	int id = 0;
	Id * idp = 0;

	if ( freeCount_ == 0 )
	{
		Q_ASSERT( first_ == 0 );
		Q_ASSERT( last_ == 0 );

		id = ids_.size();

		if ( limit_ != -1 && id == limit_ + 1 )
		{
			// no free ids left
			return 0;
		}

		if ( id >= ids_.capacity() )
			ids_.reserve( limit_ == -1 ? id + DefaultEnlargeSize : qMin( limit_ + 1, ids_.capacity() + DefaultEnlargeSize ) );

		ids_.resize( id + 1 );

		idp = ids_.data();
	}
	else
	{
		Q_ASSERT( first_ != 0 );
		Q_ASSERT( last_ != 0 );

		freeCount_--;

		idp = ids_.data();

		// take free id from start
		id = first_;

		Id & currentId = idp[ id ];
		Q_ASSERT( currentId.isFree );

		first_ = currentId.next;
		if ( first_ != 0 )
		{
			Q_ASSERT( freeCount_ > 0 );
			Q_ASSERT( id != last_ );

			Id & firstId = idp[ first_ ];
			firstId.previous = 0;
		}
		else
		{
			Q_ASSERT( freeCount_ == 0 );
			Q_ASSERT( id == last_ );

			last_ = 0;
		}
	}

	_makeIdTaken( id, idp );

	return id;
}


/** \internal
 */

inline void IdGeneratorPrivate::free( int id )
{
	Q_ASSERT( id > 0 && id < ids_.size() );

	Id * idp = ids_.data();

	Id & currentId = idp[ id ];
	Q_ASSERT( !currentId.isFree );
	currentId.isFree = true;

	if ( currentId.previous )
		idp[ currentId.previous ].next = currentId.next;
	if ( currentId.next )
		idp[ currentId.next ].previous = currentId.previous;
	if ( id == firstTaken_ )
		firstTaken_ = currentId.next;
	if ( id == lastTaken_ )
		lastTaken_ = currentId.previous;

	// put free id to begin
	if ( first_ != 0 )
	{
		Q_ASSERT( freeCount_ != 0 );
		Q_ASSERT( last_ != 0 );

		Id & firstId = idp[ first_ ];
		firstId.previous = id;
		currentId.next = first_;
		currentId.previous = 0;
		first_ = id;
	}
	else
	{
		Q_ASSERT( freeCount_ == 0 );
		Q_ASSERT( last_ == 0 );

		currentId.previous = 0;
		currentId.next = 0;
		first_ = id;
		last_ = id;
	}

	freeCount_++;
}


/** \internal
 */

inline void IdGeneratorPrivate::reserve( int id )
{
	Q_ASSERT( id > 0 );
	Q_ASSERT( limit_ == -1 || id <= limit_ );

	Id * idp = 0;

	if ( id >= ids_.size() )
	{
		if ( id >= ids_.capacity() )
			ids_.reserve( limit_ == -1 ? id + DefaultEnlargeSize : qMin( limit_ + 1, ids_.capacity() + DefaultEnlargeSize ) );

		const int firstId = ids_.size();
		ids_.resize( id + 1 );

		idp = ids_.data();
		for ( int i = firstId; i <= id; ++i )
		{
			idp[ i ].isFree = true;
			idp[ i ].previous = i - 1;
			idp[ i ].next = i + 1;
		}

		// link first new id with the last previous
		idp[ firstId ].previous = last_;
		if ( last_ != 0 )
		{
			idp[ last_ ].next = firstId;
		}
		else
		{
			first_ = firstId;
		}

		last_ = id;
		idp[ last_ ].next = 0;

		freeCount_ += id + 1 - firstId;
	}

	freeCount_--;

	if ( !idp )
		idp = ids_.data();

	Id & currentId = idp[ id ];
	Q_ASSERT_X( currentId.isFree,
		"Grim::IdGeneratorPrivate::reserve()",
		"Attempt to reserve already taken id." );

	if ( currentId.previous != 0 )
		idp[ currentId.previous ].next = currentId.next;

	if ( currentId.next != 0 )
		idp[ currentId.next ].previous = currentId.previous;

	if ( id == first_ )
		first_ = currentId.next;

	if ( id == last_ )
		last_ = currentId.previous;

	_makeIdTaken( id, idp );

	Q_ASSERT(
		(freeCount_ == 0 && first_ == 0 && last_ == 0) ||
		(freeCount_ != 0 && first_ != 0 && last_ != 0) );
}


/** \internal
 */

inline bool IdGeneratorPrivate::isFree( int id ) const
{
	Q_ASSERT( id > 0 );
	Q_ASSERT( limit_ == -1 || id <= limit_ );

	if ( id >= ids_.size() )
		return true;

	return ids_.at( id ).isFree;
}




/**
 * \class IdGenerator
 *
 * \ingroup tools_module
 *
 * \brief The IdGenerator class manages unique identifiers.
 *
 * All variables needs a unique identifiers that those variable can be referenced to.
 * Sometimes pointers are suitable, because in current process space pointers are unique.
 * Sometimes string variables and indexes are unique too.
 * IdGenerator class always returns a unique identifier from the given range for a constant time O(1).
 *
 * For example you have some items in hash and want to reference them by unique numbers:
 *
 * \code
 * QHash<int,Item> items;
 * IdGenerator generator;
 * ...
 * int createItem()
 * {
 *     int id = generator.take(); // generates a unique id and marks it as used
 *     items[ id ] = Item();
 *     return id;
 * }
 * \endcode
 *
 * Later when this item removed from the hash:
 *
 * \code
 * void removeItem( int id )
 * {
 *     generator.free( id ); // marks the given id as unused
 *     items.remove( id );
 * }
 * \endcode
 */


/**
 * Constructs generator instance with the maximum identifiers to take to be the given \a limit.
 * Passing \c -1 as \a limit value constructs generator with unlimited number identifiers to take.
 */

IdGenerator::IdGenerator( int limit ) :
	d_( new IdGeneratorPrivate( limit ) )
{
}


/**
 * Constructs generator instance from the given copy \a generator.
 */

IdGenerator::IdGenerator( const IdGenerator & generator ) :
	d_( generator.d_ )
{
}


/**
 * Assigns this generator to the given \a generator, creating exact copy from it.
 */

IdGenerator & IdGenerator::operator=( const IdGenerator & generator )
{
	d_ = generator.d_;
	return *this;
}


/**
 * Destroys generator.
 */

IdGenerator::~IdGenerator()
{
}


/**
 * Returns maximum number of ids that can be taken.
 * Returns \c -1 if ids count unlimited.
 */

int IdGenerator::limit() const
{
	return d_->limit_;
}


/**
 * Returns a unique identifier and marks it as used so it will never be returned again unless been freed with free().
 * Returns \c 0 if no ids limit was reached.
 *
 * \sa free(), reserve(), limit()
 */

int IdGenerator::take()
{
	return d_->take();
}


/**
 * Marks previously taken \a id identifier as unused so you can reuse it later.
 * You cannot free an identifier that has not been dedicated with take() earlier.
 *
 * \sa take()
 */

void IdGenerator::free( int id )
{
	return d_->free( id );
}


/**
 * Marks identifier with concrete \a id as used so it will never be returned again unless freed with free().
 * Use this method instead of take() if you know that \a id is currently unused.
 * For example when you saved unique identifiers earlier and want to reuse them in clean generator.
 * Reserving already taken id or id out of limit range is prohibited.
 *
 * \sa take(), free(), limit()
 */

void IdGenerator::reserve( int id )
{
	d_->reserve( id );
}


/**
 * Returns \c true whether the given \a id was not taken.
 * Returns \c false otherwise.
 */

bool IdGenerator::isFree( int id ) const
{
	return d_->isFree( id );
}


/**
 * Returns current count of identifiers taken with take() or reserve() and not freed with free() yet.
 */

int IdGenerator::count() const
{
	return d_->ids_.size() - d_->freeCount_ - 1;
}


/**
 * Returns \c true if no identifiers taken. Equivalent to count() == 0.
 */

bool IdGenerator::isEmpty() const
{
	return d_->freeCount_ + 1 == d_->ids_.size();
}




class IdGeneratorIteratorPrivate
{
public:
	inline IdGeneratorIteratorPrivate( const IdGeneratorPrivate * idGeneratorPrivate ) :
		idGeneratorPrivate_( idGeneratorPrivate ), currentId_( idGeneratorPrivate->firstTaken_ )
	{}

private:
	const IdGeneratorPrivate * idGeneratorPrivate_;
	int currentId_;

	friend class IdGeneratorIterator;
};




/**
 * \class IdGeneratorIterator
 *
 * \ingroup tools_module
 *
 * \brief The IdGeneratorIterator class iterator over identifiers of IdGenerator.
 *
 * Iterator allows to quickly iterate over all taken identifiers of IdGenerator instance.
 *
 * Example:
 *
 * \code
 * IdGenerator generator;
 * generator.take(); // returns 1
 * generator.take(); // returns 2
 * ...
 * for ( IdGeneratorIterator it( generator ); it.hasNext(); )
 *     qDebug() << "id =" << it.next();
 *
 * // produces:
 * // id = 1
 * // id = 2
 * \endcode
 */


/**
 * Constructs iterator that will iterate on identifiers of \a idGenerator.
 */

IdGeneratorIterator::IdGeneratorIterator( const IdGenerator & idGenerator ) :
	d_( new IdGeneratorIteratorPrivate( idGenerator.d_ ) )
{
}


/**
 * Destroys the iterator.
 */

IdGeneratorIterator::~IdGeneratorIterator()
{
	delete d_;
}


/**
 * Returns \c true if iterator not reached end, otherwise returns \c false.
 *
 * \sa next()
 */

bool IdGeneratorIterator::hasNext() const
{
	return d_->currentId_ != 0;
}


/**
 * Advances iterator and returns current id.
 * Advancing beyond the end is not allowed.
 *
 * \sa hasNext()
 */

int IdGeneratorIterator::next()
{
	const int savedCurrentId = d_->currentId_;
	d_->currentId_ = d_->idGeneratorPrivate_->ids_.at( d_->currentId_ ).next;
	return savedCurrentId;
}




} // namespace Grim
