// portable debugging helper functions specific to the STL.
// Copyright (c) 2004-2005 Jan Wassenberg
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// Contact info:
//   Jan.Wassenberg@stud.uni-karlsruhe.de
//   http://www.stud.uni-karlsruhe.de/~urkt/

#include "precompiled.h"

#include "debug_stl.h"
#include "lib.h"	// match_wildcard

// used in stl_simplify_name.
// note: strcpy is safe because replacement happens in-place and
// src is longer than dst (otherwise, we wouldn't be replacing).
#define REPLACE(what, with)\
	else if(!strncmp(src, (what), sizeof(what)-1))\
	{\
	src += sizeof(what)-1-1; /* see preincrement rationale*/\
	strcpy(dst, (with)); /* safe - see above */\
	dst += sizeof(with)-1;\
	}
#define STRIP(what)\
	else if(!strncmp(src, (what), sizeof(what)-1))\
	{\
	src += sizeof(what)-1-1;/* see preincrement rationale*/\
	}

// reduce complicated STL names to human-readable form (in place).
// e.g. "std::basic_string<char, char_traits<char>, std::allocator<char> >" =>
//  "string". algorithm: strip undesired strings in one pass (fast).
// called from symbol_string_build.
//
// see http://www.bdsoft.com/tools/stlfilt.html and
// http://www.moderncppdesign.com/publications/better_template_error_messages.html
void stl_simplify_name(char* name)
{
	// used when stripping everything inside a < > to continue until
	// the final bracket is matched (at the original nesting level).
	int nesting = 0;

	const char* src = name-1;	// preincremented; see below.
	char* dst = name;

	// for each character: (except those skipped as parts of strings)
	for(;;)
	{
		int c = *(++src);
		// preincrement rationale: src++ with no further changes would
		// require all comparisons to subtract 1. incrementing at the
		// end of a loop would require a goto, instead of continue
		// (there are several paths through the loop, for speed).
		// therefore, preincrement. when skipping strings, subtract
		// 1 from the offset (since src is advanced directly after).

		// end of string reached - we're done.
		if(c == '\0')
		{
			*dst = '\0';
			break;
		}

		// we're stripping everything inside a < >; eat characters
		// until final bracket is matched (at the original nesting level).
		if(nesting)
		{
			if(c == '<')
				nesting++;
			else if(c == '>')
			{
				nesting--;
				debug_assert(nesting >= 0);
			}
			continue;
		}

		// start if chain (REPLACE and STRIP use else if)
		if(0) {}
		else if(!strncmp(src, "::_Node", 7))
		{
			// add a space if not already preceded by one
			// (prevents replacing ">::_Node>" with ">>")
			if(src != name && src[-1] != ' ')
				*dst++ = ' ';
			src += 7;
		}
		REPLACE("unsigned short", "u16")
		REPLACE("unsigned int", "uint")
		REPLACE("unsigned __int64", "u64")
		STRIP(",0> ")
		// early out: all tests after this start with s, so skip them
		else if(c != 's')
		{
			*dst++ = c;
			continue;
		}
		REPLACE("std::_List_nod", "list")
		REPLACE("std::_Tree_nod", "map")
		REPLACE("std::basic_string<char,", "string<")
		REPLACE("std::basic_string<unsigned short,", "wstring<")
		STRIP("std::char_traits<char>,")
		STRIP("std::char_traits<unsigned short>,")
		STRIP("std::_Tmap_traits")
		STRIP("std::_Tset_traits")
		else if(!strncmp(src, "std::allocator<", 15))
		{
			// remove preceding comma (if present)
			if(src != name && src[-1] == ',')
				dst--;
			src += 15;
			// strip everything until trailing > is matched
			debug_assert(nesting == 0);
			nesting = 1;
		}
		else if(!strncmp(src, "std::less<", 10))
		{
			// remove preceding comma (if present)
			if(src != name && src[-1] == ',')
				dst--;
			src += 10;
			// strip everything until trailing > is matched
			debug_assert(nesting == 0);
			nesting = 1;
		}
		STRIP("std::")
		else
		*dst++ = c;
	}
}


//-----------------------------------------------------------------------------
// STL container debugging
//-----------------------------------------------------------------------------

// provide an iterator interface for arbitrary STL containers; this is
// used to display their contents in stack traces. their type and
// contents aren't known until runtime, so this is somewhat tricky.
//
// we assume STL containers aren't specialized on their content type and
// use their int instantiations's memory layout. vector<bool> will therefore
// not be displayed correctly, but it is frowned upon anyway (since
// address of its elements can't be taken).
// to be 100% correct, we'd have to write an Any_container_type__element_type
// class for each combination, but that is clearly infeasible.
//
// containers might still be uninitialized when we call get_container_info on
// them. we need to check if they are valid and only then use their contents.
// to that end, we derive a validator class from each container,
// cast the container's address to it, and call its valid() method.
//
// checks performed include: is size() realistic; does begin() come before
// end(), etc. we need to leverage all invariants because the values are
// random in release mode.
//
// rationale:
// - we need a complete class for each container type because the
//   valid() function sometimes needs access to protected members of
//   the containers. since we can't grant access via friend without the
//   cooperation of the system headers, it needs to be in a derived class.
// - since we cast our validator on top of the actual container,
//   it must not contain virtual functions (the vptr would shift addresses;
//   we can't really correct for this because it's totally non-portable).
// - we don't bother with making this a template because there are enough
//   variations that we'd have to specialize everything anyway.

// basic sanity checks shared by all containers.
static bool container_valid(const void* front, size_t el_count)
{
	// # elements is unbelievably high; assume it's invalid.
	if(el_count > 0x1000000)
		return false;
	if(debug_is_bogus_pointer(front))
		return false;
	return true;
}

//----------------------------------------------------------------------------

//
// standard containers
//

class Any_deque : public std::deque<int>
{
	// being declared as friend isn't enough;
	// our iterator still doesn't get access to std::deque.
	const u8* get_item(size_t i, size_t el_size) const
	{
		const u8** map = (const u8**)_Map;
		const size_t el_per_bucket = MAX(16 / el_size, 1);
		const size_t bucket_idx = i / el_per_bucket;
		const size_t idx_in_bucket = i - bucket_idx * el_per_bucket;
		const u8* bucket = map[bucket_idx];
		const u8* p = bucket + idx_in_bucket*el_size;
		return p;
	}

public:
	bool valid(size_t el_size) const
	{
		// note: front() fails on empty deques, so don't use that if
		// the container is empty (which must not be reported as invalid)
		const size_t el_count_ = el_count(el_size);
		if(el_count_ && !container_valid(&front(), el_count_))
			return false;

#if STL_DINKUMWARE != 0
		const size_t el_per_bucket = MAX(16 / el_size, 1);	// see _DEQUESIZ
		// initial element is beyond end of first bucket
		if(_Myoff >= el_per_bucket)
			return false;
		// more elements reported than fit in all buckets
		if(_Mysize > _Mapsize * el_per_bucket)
			return false;
#endif
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}

	class iter;
	friend class iter;
	class iter : public const_iterator
	{
	public:
		const u8* deref(size_t el_size)
		{
			Any_deque* d = (Any_deque*)_Mycont;
			return d->get_item(_Myoff, el_size);
		}
	};
};

class Any_list: public std::list<int>
{
public:
	bool valid(size_t el_size) const
	{
		if(!container_valid(&front(), el_count(el_size)))
			return false;
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}
};

class Any_map : public std::map<int,int>
{
	// return reference to the given node's nil flag.
	// reimplemented because this member is stored after _Myval, so it's
	// dependent on el_size.
	static _Charref _Isnil(_Nodeptr _Pnode, size_t el_size)
	{
		const u8* p = (const u8*)&_Pnode->_Isnil;	// ok for int specialization
		p += el_size - sizeof(value_type);
			// account for el_size difference
		assert(*p <= 1);	// bool value
		return (_Charref)*p;
	}

public:
	bool valid(size_t el_size) const
	{
		const_iterator it = begin();
		if(!container_valid(&*it, el_count(el_size)))
			return false;
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}

	class iter;
	friend class iter;
	class iter : public const_iterator
	{
	public:

		// move to next node (i.e. larger value)
		void advance(size_t el_size)
		{
			// end() shouldn't be incremented, don't move
			if(_Isnil(_Ptr, el_size))
				return;

			// return smallest (leftmost) node of right subtree
			_Nodeptr _Pnode = _Right(_Ptr);
			if(!_Isnil(_Pnode, el_size))
			{
				while(!_Isnil(_Left(_Pnode), el_size))
					_Pnode = _Left(_Pnode);
			}
			// climb looking for right subtree
			else
			{
				while (!_Isnil(_Pnode = _Parent(_Ptr), el_size)
					&& _Ptr == _Right(_Pnode))
					_Ptr = _Pnode;	// ==> parent while right subtree
			}
			_Ptr = _Pnode;
		}

	};
};


class Any_multimap : public Any_map
{
};

class Any_set: public std::set<int>
{
public:
	bool valid(size_t el_size) const
	{
		const_iterator it = begin();
		if(!container_valid(&*it, el_count(el_size)))
			return false;
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}
};

class Any_multiset: public Any_set
{
};

class Any_vector: public std::vector<int>
{
public:
	bool valid(size_t el_size) const
	{
		const size_t el_count_ = el_count(el_size);
		// note: front() will be 0 if container is empty, but
		// that must not be reported as invalid.
		if(el_count_ && !container_valid(&front(), el_count_))
			return false;
		// more elements reported than reserved
		if(size() > capacity())
			return false;
		// front/back pointers incorrect
		if(&front() > &back())
			return false;
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		// vectors store front and back pointers and calculate
		// element count as the difference between them. since we are
		// derived from the int specialization, the pointer arithmetic is
		// off. we fix it by taking el_size into account.
		return size() * 4 / el_size;
	}

	class iter;
	friend class iter;
	class iter : public const_iterator
	{
	public:

		// move to next item
		void advance(size_t el_size)
		{
			_Myptr = (_Tptr)((u8*)_Myptr + el_size);
		}
	};
};

class Any_basic_string : public std::string
{
public:
	bool valid(size_t el_size) const
	{
		if(!container_valid(c_str(), el_count(el_size)))
			return false;
#if STL_DINKUMWARE != 0
		// less than the small buffer reserved - impossible
		if(_Myres < (16/el_size)-1)
			return false;
		// more elements reported than reserved
		if(_Mysize > _Myres)
			return false;
#endif
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}
};


//
// standard container adapters
//

// we assume this adapter was instantiated with container=deque!
class Any_queue : public Any_deque
{
};

// we assume this adapter was instantiated with container=deque!
class Any_stack : public Any_deque
{
};

//
// nonstandard containers (will probably be part of C++0x)
//

#ifdef HAVE_STL_HASH

class Any_hash_map: public STL_HASH_MAP<int,int>
{
public:
	bool valid(size_t el_size) const
	{
		const_iterator it = begin();
		if(!container_valid(&*it, el_count(el_size)))
			return false;
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}
};

class Any_hash_multimap : public Any_hash_map
{
};

class Any_hash_set: public STL_HASH_SET<int>
{
public:
	bool valid(size_t el_size) const
	{
		const_iterator it = begin();
		if(!container_valid(&*it, el_count(el_size)))
			return false;
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}
};

class Any_hash_multiset : public Any_hash_set
{
};

#endif	// HAVE_STL_HASH

#ifdef HAVE_STL_SLIST

class Any_slist: public STL_SLIST<int>
{
public:
	bool valid(size_t el_size) const
	{
		if(!container_valid(&front(), el_count(el_size)))
			return false;
		return true;
	}

	size_t el_count(size_t el_size) const
	{
		UNUSED(el_size);
		return size();
	}
};

#endif	// HAVE_STL_SLIST

//-----------------------------------------------------------------------------

// generic iterator - returns next element. dereferences and increments the
// specific container iterator stored in it_mem.
template<class T> const u8* stl_iterator(void* it_mem, size_t el_size)
{
	UNUSED(el_size);
	T::const_iterator* const p_cit = (T::const_iterator*)it_mem;
	T::const_reference el = p_cit->operator*();
	const u8* p = (const u8*)&el;
	p_cit->operator++();
	return p;
}

// vector iterators advance by sizeof(value_type) bytes; since we assume the
// int specialization, we have to do this ourselves.
template<> const u8* stl_iterator<Any_vector>(void* it_mem, size_t el_size)
{
	Any_vector::iter* pi = (Any_vector::iter*)it_mem;
	const u8* p = (const u8*)&*(*pi);
	pi->advance(el_size);
	return p;
}

// deque iterator operator* depends on el_size
#if STL_DINKUMWARE != 0
template<> const u8* stl_iterator<Any_deque>(void* it_mem, size_t el_size)
{
	Any_deque::iter* pi = (Any_deque::iter*)it_mem;
	const u8* p = pi->deref(el_size);
	++(*pi);
	return p;
}
#endif

// map iterator operator++ depends on el_size
#if STL_DINKUMWARE != 0
template<> const u8* stl_iterator<Any_map>(void* it_mem, size_t el_size)
{
	Any_map::iter* pi = (Any_map::iter*)it_mem;
	const u8* p = (const u8*)&*(*pi);
	pi->advance(el_size);
	return p;
}
#endif

//-----------------------------------------------------------------------------

// check if the container is valid and return # elements and an iterator;
// this is instantiated once for each type of container.
// we don't do this in the Any_* ctors because we need to return bool valid and
// don't want to throw an exception (may confuse the debug code).
template<class T> bool get_container_info(T* t, size_t size, size_t el_size,
	size_t* el_count, DebugIterator* el_iterator, void* it_mem)
{
	debug_assert(sizeof(T) == size);
	debug_assert(sizeof(T::iterator) < DEBUG_STL_MAX_ITERATOR_SIZE);

	*el_count = t->el_count(el_size);
	*el_iterator = stl_iterator<T>;
	*(T::const_iterator*)it_mem = t->begin();
	return t->valid(el_size);
}


// if <wtype_name> indicates the object <p, size> to be an STL container,
// and given the size of its value_type (retrieved via debug information),
// return number of elements and an iterator (any data it needs is stored in
// it_mem, which must hold DEBUG_STL_MAX_ITERATOR_SIZE bytes).
// returns 0 on success or an StlContainerError.
int stl_get_container_info(const wchar_t* wtype_name, const u8* p, size_t size,
	size_t el_size, size_t* el_count, DebugIterator* el_iterator, void* it_mem)
{
	// HACK: The debug_stl code breaks VS2005's STL badly, causing crashes in
	// later pieces of code that try to manipulate the STL containers. Presumably
	// it needs to be altered/rewritten to work happily with the new STL debug iterators.
#if defined(_MSC_VER) && _MSC_VER >= 1400
	return -1;
#endif

	bool handled = false, valid = false;
#define CONTAINER(name, type_name_pattern)\
	else if(match_wildcard(type_name, type_name_pattern))\
	{\
		handled = true;\
		valid = get_container_info<Any_##name>((Any_##name*)p, size, el_size, el_count, el_iterator, it_mem);\
	}
#define STD_CONTAINER(name) CONTAINER(name, "std::" #name "<*>")

	// workaround for preprocessor limitation: what we're trying to do is
	// stringize the defined value of a macro. prepending and pasting L
	// apparently isn't possible because macro args aren't expanded before
	// being pasted; we therefore convert to char[] and compare against that.
	char type_name[DBG_SYMBOL_LEN];
	snprintf(type_name, ARRAY_SIZE(type_name), "%ws", wtype_name);
#define STRINGIZE2(id) # id
#define STRINGIZE(id) STRINGIZE2(id)

	if(0) {}	// kickoff
	// standard containers
	STD_CONTAINER(deque)	// deref provided
	STD_CONTAINER(list)		// ok
	STD_CONTAINER(map)		// advance provided
	STD_CONTAINER(multimap)	// advance provided
	STD_CONTAINER(set)		// TODO use map impl?
	STD_CONTAINER(multiset)	// TODO use map impl?
	STD_CONTAINER(vector)	// special-cased
	STD_CONTAINER(basic_string)	// ok
	// standard container adapter
	STD_CONTAINER(queue)	// not ok (deque)
	STD_CONTAINER(stack)	// not ok (deque)
	// nonstandard containers (will probably be part of C++0x)
#ifdef HAVE_STL_HASH
	CONTAINER(hash_map, STRINGIZE(STL_HASH_MAP) "<*>")
	CONTAINER(hash_multimap, STRINGIZE(STL_HASH_MULTIMAP) "<*>")
	CONTAINER(hash_set, STRINGIZE(STL_HASH_SET) "<*>")
	CONTAINER(hash_multiset, STRINGIZE(STL_HASH_MULTISET) "<*>")
#endif
#ifdef HAVE_STL_SLIST
	CONTAINER(slist, STRINGIZE(STL_SLIST) "<*>")
#endif

	if(!handled)
		return STL_CNT_UNKNOWN;
	if(!valid)
		return STL_CNT_INVALID;
	return 0;
}
