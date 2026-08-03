/*
===========================================================================

DarklightNG Source Code
Copyright (C) 2026 - Justin Marshall(aka IceColdDuke).

This file is part of the DarklightNG GPL source code.
This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

DarklightNG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DarklightNG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

===========================================================================
*/

#ifndef __LIST_H__
#define __LIST_H__

#include <algorithm>
#include <vector>

/*
===============================================================================

	List template
	Does not allocate memory until the first item is added.

===============================================================================
*/


/*
================
idListSortCompare<type>
================
*/
#ifdef __INTEL_COMPILER
// the intel compiler doesn't do the right thing here
template< class type >
ID_INLINE int idListSortCompare( const type *a, const type *b ) {
	assert( 0 );
	return 0;
}
#else
template< class type >
ID_INLINE int idListSortCompare( const type *a, const type *b ) {
	return *a - *b;
}
#endif

/*
================
idListNewElement<type>
================
*/
template< class type >
ID_INLINE type *idListNewElement( void ) {
	return new type;
}

/*
================
idSwap<type>
================
*/
template< class type >
ID_INLINE void idSwap( type &a, type &b ) {
	type c = a;
	a = b;
	b = c;
}

template< class type >
class idList {
public:

	typedef int		cmp_t( const type *, const type * );
	typedef type	new_t( void );
	typedef typename std::vector<type>::reference reference;
	typedef typename std::vector<type>::const_reference const_reference;

					idList( int newgranularity = 16 );
					idList( const idList<type> &other );
					~idList<type>( void );

	void			Clear( void );										// clear the list
	int				Num( void ) const;									// returns number of elements in list
	int				NumAllocated( void ) const;							// returns number of elements allocated for
	void			SetGranularity( int newgranularity );				// set new granularity
	int				GetGranularity( void ) const;						// get the current granularity

	size_t			Allocated( void ) const;							// returns total size of allocated memory
	size_t			Size( void ) const;									// returns total size of allocated memory including size of list type
	size_t			MemoryUsed( void ) const;							// returns size of the used elements in the list

	idList<type> &	operator=( const idList<type> &other );
	const_reference	operator[]( int index ) const;
	reference		operator[]( int index );

	void			Condense( void );									// resizes list to exactly the number of elements it contains
	void			Resize( int newsize );								// resizes list to the given number of elements
	void			Resize( int newsize, int newgranularity	 );			// resizes list and sets new granularity
	void			SetNum( int newnum, bool resize = true );			// set number of elements in list and resize to exactly this number if necessary
	void			AssureSize( int newSize);							// assure list has given number of elements, but leave them uninitialized
	void			AssureSize( int newSize, const type &initValue );	// assure list has given number of elements and initialize any new elements
	void			AssureSizeAlloc( int newSize, new_t *allocator );	// assure the pointer list has the given number of elements and allocate any new elements

	type *			Ptr( void );										// returns a pointer to the list
	const type *	Ptr( void ) const;									// returns a pointer to the list
	reference		Alloc( void );										// returns reference to a new data element at the end of the list
	int				Append( const type & obj );							// append element
	int				Append( const idList<type> &other );				// append list
	int				AddUnique( const type & obj );						// add unique element
	int				Insert( const type & obj, int index = 0 );			// insert the element at the given index
	int				FindIndex( const type & obj ) const;				// find the index for the given element
	type *			Find( type const & obj ) const;						// find pointer to the given element
	int				FindNull( void ) const;								// find the index for the first NULL pointer in the list
	int				IndexOf( const type *obj ) const;					// returns the index for the pointer to an element in the list
	bool			RemoveIndex( int index );							// remove the element at the given index
	bool			Remove( const type & obj );							// remove the element
	void			Sort( cmp_t *compare = ( cmp_t * )&idListSortCompare<type> );
	void			SortSubSection( int startIndex, int endIndex, cmp_t *compare = ( cmp_t * )&idListSortCompare<type> );
	void			Swap( idList<type> &other );						// swap the contents of the lists
	void			DeleteContents( bool clear );						// delete the contents of the list

private:
	int				num;
	int				granularity;
	std::vector<type>	list;
};

/*
================
idList<type>::idList( int )
================
*/
template< class type >
ID_INLINE idList<type>::idList( int newgranularity ) : num( 0 ), granularity( newgranularity ) {
	assert( newgranularity > 0 );
}

/*
================
idList<type>::idList( const idList<type> &other )
================
*/
template< class type >
ID_INLINE idList<type>::idList( const idList<type> &other ) : num( other.num ), granularity( other.granularity ), list( other.list ) {
}

/*
================
idList<type>::~idList<type>
================
*/
template< class type >
ID_INLINE idList<type>::~idList( void ) {
}

/*
================
idList<type>::Clear

Frees up the memory allocated by the list.  Assumes that type automatically handles freeing up memory.
================
*/
template< class type >
ID_INLINE void idList<type>::Clear( void ) {
	std::vector<type>().swap( list );
	num = 0;
}

/*
================
idList<type>::DeleteContents

Calls the destructor of all elements in the list.  Conditionally frees up memory used by the list.
Note that this only works on lists containing pointers to objects and will cause a compiler error
if called with non-pointers.  Since the list was not responsible for allocating the object, it has
no information on whether the object still exists or not, so care must be taken to ensure that
the pointers are still valid when this function is called.  Function will set all pointers in the
list to NULL.
================
*/
template< class type >
ID_INLINE void idList<type>::DeleteContents( bool clear ) {
	for( int i = 0; i < num; i++ ) {
		delete list[ i ];
		list[ i ] = type();
	}

	if ( clear ) {
		Clear();
	} else {
		std::fill( list.begin(), list.end(), type() );
	}
}

/*
================
idList<type>::Allocated

return total memory allocated for the list in bytes, but doesn't take into account additional memory allocated by type
================
*/
template< class type >
ID_INLINE size_t idList<type>::Allocated( void ) const {
	return list.size() * sizeof( type );
}

/*
================
idList<type>::Size

return total size of list in bytes, but doesn't take into account additional memory allocated by type
================
*/
template< class type >
ID_INLINE size_t idList<type>::Size( void ) const {
	return sizeof( idList<type> ) + Allocated();
}

/*
================
idList<type>::MemoryUsed
================
*/
template< class type >
ID_INLINE size_t idList<type>::MemoryUsed( void ) const {
	return num * sizeof( type );
}

/*
================
idList<type>::Num

Returns the number of elements currently contained in the list.
Note that this is NOT an indication of the memory allocated.
================
*/
template< class type >
ID_INLINE int idList<type>::Num( void ) const {
	return num;
}

/*
================
idList<type>::NumAllocated

Returns the number of elements currently allocated for.
================
*/
template< class type >
ID_INLINE int idList<type>::NumAllocated( void ) const {
	return static_cast<int>( list.size() );
}

/*
================
idList<type>::SetNum

Resize to the exact size specified irregardless of granularity
================
*/
template< class type >
ID_INLINE void idList<type>::SetNum( int newnum, bool resize ) {
	assert( newnum >= 0 );
	if ( resize || newnum > NumAllocated() ) {
		Resize( newnum );
	}
	num = newnum;
}

/*
================
idList<type>::SetGranularity

Sets the base size of the array and resizes the array to match.
================
*/
template< class type >
ID_INLINE void idList<type>::SetGranularity( int newgranularity ) {
	int newsize;

	assert( newgranularity > 0 );
	granularity = newgranularity;

	if ( !list.empty() ) {
		// resize it to the closest level of granularity
		newsize = num + granularity - 1;
		newsize -= newsize % granularity;
		if ( newsize != NumAllocated() ) {
			Resize( newsize );
		}
	}
}

/*
================
idList<type>::GetGranularity

Get the current granularity.
================
*/
template< class type >
ID_INLINE int idList<type>::GetGranularity( void ) const {
	return granularity;
}

/*
================
idList<type>::Condense

Resizes the array to exactly the number of elements it contains or frees up memory if empty.
================
*/
template< class type >
ID_INLINE void idList<type>::Condense( void ) {
	if ( !list.empty() ) {
		if ( num ) {
			Resize( num );
		} else {
			Clear();
		}
	}
}

/*
================
idList<type>::Resize

Allocates memory for the amount of elements requested while keeping the contents intact.
Contents are copied using their = operator so that data is correnctly instantiated.
================
*/
template< class type >
ID_INLINE void idList<type>::Resize( int newsize ) {
	assert( newsize >= 0 );

	// free up the list if no data is being reserved
	if ( newsize <= 0 ) {
		Clear();
		return;
	}

	if ( newsize == NumAllocated() ) {
		// not changing the size, so just exit
		return;
	}

	if ( newsize < num ) {
		num = newsize;
	}

	list.resize( newsize );
}

/*
================
idList<type>::Resize

Allocates memory for the amount of elements requested while keeping the contents intact.
Contents are copied using their = operator so that data is correnctly instantiated.
================
*/
template< class type >
ID_INLINE void idList<type>::Resize( int newsize, int newgranularity ) {
	assert( newsize >= 0 );

	assert( newgranularity > 0 );
	granularity = newgranularity;

	Resize( newsize );
}

/*
================
idList<type>::AssureSize

Makes sure the list has at least the given number of elements.
================
*/
template< class type >
ID_INLINE void idList<type>::AssureSize( int newSize ) {
	int newNum = newSize;

	if ( newSize > NumAllocated() ) {
		newSize += granularity - 1;
		newSize -= newSize % granularity;
		Resize( newSize );
	}

	num = newNum;
}

/*
================
idList<type>::AssureSize

Makes sure the list has at least the given number of elements and initialize any elements not yet initialized.
================
*/
template< class type >
ID_INLINE void idList<type>::AssureSize( int newSize, const type &initValue ) {
	int newNum = newSize;

	if ( newSize > NumAllocated() ) {
		newSize += granularity - 1;
		newSize -= newSize % granularity;
		num = NumAllocated();
		Resize( newSize );

		for ( int i = num; i < newSize; i++ ) {
			list[i] = initValue;
		}
	}

	num = newNum;
}

/*
================
idList<type>::AssureSizeAlloc

Makes sure the list has at least the given number of elements and allocates any elements using the allocator.

NOTE: This function can only be called on lists containing pointers. Calling it
on non-pointer lists will cause a compiler error.
================
*/
template< class type >
ID_INLINE void idList<type>::AssureSizeAlloc( int newSize, new_t *allocator ) {
	int newNum = newSize;

	if ( newSize > NumAllocated() ) {
		newSize += granularity - 1;
		newSize -= newSize % granularity;
		num = NumAllocated();
		Resize( newSize );

		for ( int i = num; i < newSize; i++ ) {
			list[i] = (*allocator)();
		}
	}

	num = newNum;
}

/*
================
idList<type>::operator=

Copies the contents and size attributes of another list.
================
*/
template< class type >
ID_INLINE idList<type> &idList<type>::operator=( const idList<type> &other ) {
	if ( this != &other ) {
		num = other.num;
		granularity = other.granularity;
		list = other.list;
	}

	return *this;
}

/*
================
idList<type>::operator[] const

Access operator.  Index must be within range or an assert will be issued in debug builds.
Release builds do no range checking.
================
*/
template< class type >
ID_INLINE typename idList<type>::const_reference idList<type>::operator[]( int index ) const {
	assert( index >= 0 );
	assert( index < num );

	return list[ index ];
}

/*
================
idList<type>::operator[]

Access operator.  Index must be within range or an assert will be issued in debug builds.
Release builds do no range checking.
================
*/
template< class type >
ID_INLINE typename idList<type>::reference idList<type>::operator[]( int index ) {
	assert( index >= 0 );
	assert( index < num );

	return list[ index ];
}

/*
================
idList<type>::Ptr

Returns a pointer to the begining of the array.  Useful for iterating through the list in loops.

Note: may return NULL if the list is empty.

FIXME: Create an iterator template for this kind of thing.
================
*/
template< class type >
ID_INLINE type *idList<type>::Ptr( void ) {
	return list.empty() ? NULL : &list[ 0 ];
}

/*
================
idList<type>::Ptr

Returns a pointer to the begining of the array.  Useful for iterating through the list in loops.

Note: may return NULL if the list is empty.

FIXME: Create an iterator template for this kind of thing.
================
*/
template< class type >
const ID_INLINE type *idList<type>::Ptr( void ) const {
	return list.empty() ? NULL : &list[ 0 ];
}

/*
================
idList<type>::Alloc

Returns a reference to a new data element at the end of the list.
================
*/
template< class type >
ID_INLINE typename idList<type>::reference idList<type>::Alloc( void ) {
	if ( list.empty() ) {
		Resize( granularity );
	}

	if ( num == NumAllocated() ) {
		Resize( NumAllocated() + granularity );
	}

	return list[ num++ ];
}

/*
================
idList<type>::Append

Increases the size of the list by one element and copies the supplied data into it.

Returns the index of the new element.
================
*/
template< class type >
ID_INLINE int idList<type>::Append( type const & obj ) {
	if ( list.empty() ) {
		Resize( granularity );
	}

	if ( num == NumAllocated() ) {
		int newsize;

		newsize = NumAllocated() + granularity;
		Resize( newsize - newsize % granularity );
	}

	list[ num ] = obj;
	num++;

	return num - 1;
}


/*
================
idList<type>::Insert

Increases the size of the list by at leat one element if necessary 
and inserts the supplied data into it.

Returns the index of the new element.
================
*/
template< class type >
ID_INLINE int idList<type>::Insert( type const & obj, int index ) {
	if ( list.empty() ) {
		Resize( granularity );
	}

	if ( num == NumAllocated() ) {
		int newsize;

		newsize = NumAllocated() + granularity;
		Resize( newsize - newsize % granularity );
	}

	if ( index < 0 ) {
		index = 0;
	}
	else if ( index > num ) {
		index = num;
	}
	for ( int i = num; i > index; --i ) {
		list[i] = list[i-1];
	}
	num++;
	list[index] = obj;
	return index;
}

/*
================
idList<type>::Append

adds the other list to this one

Returns the size of the new combined list
================
*/
template< class type >
ID_INLINE int idList<type>::Append( const idList<type> &other ) {
	if ( list.empty() ) {
		Resize( granularity );
	}

	int n = other.Num();
	for (int i = 0; i < n; i++) {
		Append(other[i]);
	}

	return Num();
}

/*
================
idList<type>::AddUnique

Adds the data to the list if it doesn't already exist.  Returns the index of the data in the list.
================
*/
template< class type >
ID_INLINE int idList<type>::AddUnique( type const & obj ) {
	int index;

	index = FindIndex( obj );
	if ( index < 0 ) {
		index = Append( obj );
	}

	return index;
}

/*
================
idList<type>::FindIndex

Searches for the specified data in the list and returns it's index.  Returns -1 if the data is not found.
================
*/
template< class type >
ID_INLINE int idList<type>::FindIndex( type const & obj ) const {
	int i;

	for( i = 0; i < num; i++ ) {
		if ( list[ i ] == obj ) {
			return i;
		}
	}

	// Not found
	return -1;
}

/*
================
idList<type>::Find

Searches for the specified data in the list and returns it's address. Returns NULL if the data is not found.
================
*/
template< class type >
ID_INLINE type *idList<type>::Find( type const & obj ) const {
	int i;

	i = FindIndex( obj );
	if ( i >= 0 ) {
		return const_cast<type *>( &list[ i ] );
	}

	return NULL;
}

/*
================
idList<type>::FindNull

Searches for a NULL pointer in the list.  Returns -1 if NULL is not found.

NOTE: This function can only be called on lists containing pointers. Calling it
on non-pointer lists will cause a compiler error.
================
*/
template< class type >
ID_INLINE int idList<type>::FindNull( void ) const {
	int i;

	for( i = 0; i < num; i++ ) {
		if ( list[ i ] == NULL ) {
			return i;
		}
	}

	// Not found
	return -1;
}

/*
================
idList<type>::IndexOf

Takes a pointer to an element in the list and returns the index of the element.
This is NOT a guarantee that the object is really in the list. 
Function will assert in debug builds if pointer is outside the bounds of the list,
but remains silent in release builds.
================
*/
template< class type >
ID_INLINE int idList<type>::IndexOf( type const *objptr ) const {
	int index;

	index = objptr - &list[ 0 ];

	assert( index >= 0 );
	assert( index < num );

	return index;
}

/*
================
idList<type>::RemoveIndex

Removes the element at the specified index and moves all data following the element down to fill in the gap.
The number of elements in the list is reduced by one.  Returns false if the index is outside the bounds of the list.
Note that the element is not destroyed, so any memory used by it may not be freed until the destruction of the list.
================
*/
template< class type >
ID_INLINE bool idList<type>::RemoveIndex( int index ) {
	int i;

	assert( !list.empty() );
	assert( index >= 0 );
	assert( index < num );

	if ( ( index < 0 ) || ( index >= num ) ) {
		return false;
	}

	num--;
	for( i = index; i < num; i++ ) {
		list[ i ] = list[ i + 1 ];
	}

	return true;
}

/*
================
idList<type>::Remove

Removes the element if it is found within the list and moves all data following the element down to fill in the gap.
The number of elements in the list is reduced by one.  Returns false if the data is not found in the list.  Note that
the element is not destroyed, so any memory used by it may not be freed until the destruction of the list.
================
*/
template< class type >
ID_INLINE bool idList<type>::Remove( type const & obj ) {
	int index;

	index = FindIndex( obj );
	if ( index >= 0 ) {
		return RemoveIndex( index );
	}
	
	return false;
}

/*
================
idList<type>::Sort

Performs a qsort on the list using the supplied comparison function.  Note that the data is merely moved around the
list, so any pointers to data within the list may no longer be valid.
================
*/
template< class type >
ID_INLINE void idList<type>::Sort( cmp_t *compare ) {
	if ( list.empty() ) {
		return;
	}
	std::sort( list.begin(), list.begin() + num,
		[compare]( const type &a, const type &b ) { return compare( &a, &b ) < 0; } );
}

/*
================
idList<type>::SortSubSection

Sorts a subsection of the list.
================
*/
template< class type >
ID_INLINE void idList<type>::SortSubSection( int startIndex, int endIndex, cmp_t *compare ) {
	if ( list.empty() ) {
		return;
	}
	if ( startIndex < 0 ) {
		startIndex = 0;
	}
	if ( endIndex >= num ) {
		endIndex = num - 1;
	}
	if ( startIndex >= endIndex ) {
		return;
	}
	std::sort( list.begin() + startIndex, list.begin() + endIndex + 1,
		[compare]( const type &a, const type &b ) { return compare( &a, &b ) < 0; } );
}

/*
================
idList<type>::Swap

Swaps the contents of two lists
================
*/
template< class type >
ID_INLINE void idList<type>::Swap( idList<type> &other ) {
	idSwap( num, other.num );
	idSwap( granularity, other.granularity );
	list.swap( other.list );
}

#endif /* !__LIST_H__ */
