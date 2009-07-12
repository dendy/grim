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

#include "idgenerator.h"




namespace Grim {




static const int DefaultSize = 1024;




class IdGeneratorPrivate
{
public:
	IdGeneratorPrivate( int size );

	int take();
	void free( int id );
	void reserve( int id );

private:
	void _init( int size );
	void _enlarge( int size );

private:
	struct Id
	{
		int next;
		int previous;
		bool isFree;
	};

	bool isAutoEnlarge_;
	int first_;
	int last_;
	int freeCount_;
	int size_;
	QVector<Id> ids_;

	friend class IdGenerator;
	friend class IdGeneratorIterator;
	friend class IdGeneratorIteratorPrivate;
};




inline IdGeneratorPrivate::IdGeneratorPrivate( int size ) :
	isAutoEnlarge_( true )
{
	_init( size );
}


/** \internal
 * Initializes a generator with the given \a size of table of identifiers.
 */

inline void IdGeneratorPrivate::_init( int size )
{
	// size == 0 not allowed
	Q_ASSERT( size == -1 || size > 0 );

	size_ = size == -1 ? DefaultSize : size;

	// if size == 1 then first and last is not available until
	// generator will not be enlarged
	freeCount_ = size_ - 1;
	first_ = size_ == 1 ? 0 : 1;
	last_ = size_ == 1 ? 0 : size_ - 1;

	ids_.resize( size_ );
	Id * idp = ids_.data();
	for ( int i = 1; i < size_; ++i )
	{
		idp[i].previous = i-1;
		idp[i].next = i+1;
		idp[i].isFree = true;
	}
	idp[0].previous = 0;
	idp[size_-1].next = 0;
}


/** \internal
 * Enlarges table of identifiers by \a size.
 */

inline void IdGeneratorPrivate::_enlarge( int size )
{
	Q_ASSERT( size > 0 );

	const int newSize = size_ + size;

	QVector<Id> newIds;
	newIds.resize( newSize );
	memcpy( newIds.data(), ids_.constData(), size_*sizeof(Id) );

	Id * idp = newIds.data();
	for ( int i = size_; i < newSize; ++i )
	{
		idp[i].previous = i - 1;
		idp[i].next = i + 1;
		idp[i].isFree = true;
	}

	// link new ids with previous ids
	if ( freeCount_ > 0 )
	{
		idp[size_].previous = last_;
		idp[last_].next = size_;
	}
	else
	{
		// no free ids left, mark first id from new ones as first free
		idp[size_].previous = 0;
		first_ = size_;
	}
	last_ = newSize - 1;
	idp[last_].next = 0;

	freeCount_ += size;
	ids_ = newIds;
	size_ = newSize;
}


/** \internal
 */

inline int IdGeneratorPrivate::take()
{
	if ( freeCount_ == 0 )
	{
		if ( !isAutoEnlarge_ )
		{
			// no free ids left
			return 0;
		}
		_enlarge( DefaultSize );
	}

	freeCount_--;

	Id * idp = ids_.data();

	// take free id from start
	const int id = first_;
	Q_ASSERT( id != 0 );

	Id & currentId = idp[ id ];
	Q_ASSERT( currentId.isFree );
	currentId.isFree = false;

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

	return id;
}


/** \internal
 */

inline void IdGeneratorPrivate::free( int id )
{
	Q_ASSERT( id > 0 && id < size_ );

	Id * idp = ids_.data();

	Id & currentId = idp[ id ];
	Q_ASSERT( !currentId.isFree );
	currentId.isFree = true;

	// put free id to end
	if ( last_ != 0 )
	{
		Q_ASSERT( freeCount_ != 0 );
		Q_ASSERT( first_ != 0 );

		Id & lastId = idp[ last_ ];
		lastId.next = id;
		currentId.previous = last_;
		currentId.next = 0;
		last_ = id;
	}
	else
	{
		Q_ASSERT( freeCount_ == 0 );
		Q_ASSERT( first_ == 0 );

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

	if ( id >= size_ )
	{
		Q_ASSERT_X( isAutoEnlarge_,
			"Grim::IdGeneratorPrivate::reserve()",
			"Attempt to reserve identifier that is out of allowed bounds." );

		_enlarge( size_ - id + 1 + DefaultSize );
	}

	freeCount_--;

	Id * idp = ids_.data();

	Id & currentId = idp[ id ];
	Q_ASSERT_X( currentId.isFree,
		"Grim::IdGeneratorPrivate::reserve()",
		"Attempt to reserve alreade taken id." );
	currentId.isFree = false;

	Id & previousId = idp[ currentId.previous ];
	previousId.next = 0;

	Id & nextId = idp[ currentId.next ];
	nextId.previous = 0;

	if ( id == first_ )
		first_ = currentId.next;

	if ( id == last_ )
		last_ = currentId.previous;

	Q_ASSERT( freeCount_ > 0 || (first_ == 0 && last_ == 0) );
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
 * Later when this item removed from the hash:
 * \code
 * void removeItem( int id )
 * {
 *     generator.free( id ); // marks the given id as unused
 *     items.remove( id );
 * }
 * \endcode
 */

/**
 * Constructs a generator instance with the given initial \a size of identifiers to handle.
 * If no size was given default size will be used, which is infinite because auto enlargement is on.
 * You can reinitialize generator later with reset() call.
 * \a size value only make sense together with the autoEnlarge parameter.
 *
 * \sa reset(), setAutoEnlarge()
 */

IdGenerator::IdGenerator( int size ) :
	d_( new IdGeneratorPrivate( size ) )
{
}


/**
 * Destroys generator.
 */

IdGenerator::~IdGenerator()
{
	delete d_;
}


/**
 * Returns a unique identifier and marks it as used so it will never be returned again unless been freed with free().
 * Returns \c 0 if isAutoEnlarge() is off and more free identifiers left.
 *
 * \sa free()
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
 *
 * \sa take(), free()
 */

void IdGenerator::reserve( int id )
{
	d_->reserve( id );
}


/**
 * Reinitializes a generator with the given \a size of ids.
 * Pass -1 to the \a size if you don't want to change current size.
 *
 * \sa size()
 */

void IdGenerator::reset( int size )
{
	d_->_init( size );
}


/**
 * Returns current count of identifiers taken with take() and not freed with free() yet.
 */

int IdGenerator::count() const
{
	return d_->size_ - d_->freeCount_ - 1;
}


/**
 * Returns \c true if no identifiers taken. Equivalent to count() == 0.
 */

bool IdGenerator::isEmpty() const
{
	return d_->freeCount_ + 1 == d_->size_;
}


/**
 * Returns maximum allowed number of identifiers.
 * If isAutoEnlarge() is on then size of generator can grow with take() of reserve() calls.
 * Note that identifier \c 0 is always reserved as null, so it cannot be used.
 *
 * \sa count(), reset(), isAutoEnlarge()
 */

int IdGenerator::size() const
{
	return d_->size_;
}


/**
 * Returns flag indicating that this id generator should enlarge size() when identifier
 * count exceed limit. If this flag if on then actually id generator will hold unlimited number
 * of identifiers.
 * Auto enlarging is enabled by default.
 *
 * \sa setAutoEnlarge()
 */

bool IdGenerator::isAutoEnlarge() const
{
	return d_->isAutoEnlarge_;
}


/**
 * Set auto enlargement flag to \a set.
 *
 * \sa isAutoEnlarge()
 */

void IdGenerator::setAutoEnlarge( bool set )
{
	d_->isAutoEnlarge_ = set;
}




class IdGeneratorIteratorPrivate
{
public:
	inline IdGeneratorIteratorPrivate( const IdGeneratorPrivate * idGeneratorPrivate ) :
		idGeneratorPrivate_( idGeneratorPrivate ), currentId_( idGeneratorPrivate->first_ )
	{}

private:
	const IdGeneratorPrivate * idGeneratorPrivate_;
	int currentId_;

	friend class IdGeneratorIterator;
};




/**
 * \class IdGeneratorIterator
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
