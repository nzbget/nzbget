/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef CONTAINER_H
#define CONTAINER_H

// for-range loops on pointers to std-containers (avoiding dereferencing)
template <typename T> typename std::deque<T>::iterator begin(std::deque<T>* c) { return c->begin(); }
template <typename T> typename std::deque<T>::iterator end(std::deque<T>* c) { return c->end(); }
template <typename T> typename std::vector<T>::iterator begin(std::vector<T>* c) { return c->begin(); }
template <typename T> typename std::vector<T>::iterator end(std::vector<T>* c) { return c->end(); }
template <typename T> typename std::list<T>::iterator begin(std::list<T>* c) { return c->begin(); }
template <typename T> typename std::list<T>::iterator end(std::list<T>* c) { return c->end(); }

// for-range loops on pointers to std-containers of unique_ptr:
// iterating through raw pointers instead of through unique_ptr

template <typename T>
struct RawDequeIterator
{
	RawDequeIterator(typename std::deque<std::unique_ptr<T>>::iterator baseIterator) : m_baseIterator(baseIterator) {}
	typename std::deque<std::unique_ptr<T>>::iterator m_baseIterator;
};
template <typename T> bool operator!=(RawDequeIterator<T>& it1, RawDequeIterator<T>& it2) { return it1.m_baseIterator != it2.m_baseIterator; }
template <typename T> T* operator*(RawDequeIterator<T>& it) { return (*it.m_baseIterator).get(); }
template <typename T> RawDequeIterator<T> operator++(RawDequeIterator<T>& it) { return RawDequeIterator<T>(it.m_baseIterator++); }
template <typename T> RawDequeIterator<T> begin(std::deque<std::unique_ptr<T>>* c) { return RawDequeIterator<T>(c->begin()); }
template <typename T> RawDequeIterator<T> end(std::deque<std::unique_ptr<T>>* c) { return RawDequeIterator<T>(c->end()); }

template <typename T>
struct RawVectorIterator
{
	RawVectorIterator(typename std::vector<std::unique_ptr<T>>::iterator baseIterator) : m_baseIterator(baseIterator) {}
	typename std::vector<std::unique_ptr<T>>::iterator m_baseIterator;
};
template <typename T> bool operator!=(RawVectorIterator<T>& it1, RawVectorIterator<T>& it2) { return it1.m_baseIterator != it2.m_baseIterator; }
template <typename T> T* operator*(RawVectorIterator<T>& it) { return (*it.m_baseIterator).get(); }
template <typename T> RawVectorIterator<T> operator++(RawVectorIterator<T>& it) { return RawVectorIterator<T>(it.m_baseIterator++); }
template <typename T> RawVectorIterator<T> begin(std::vector<std::unique_ptr<T>>* c) { return RawVectorIterator<T>(c->begin()); }
template <typename T> RawVectorIterator<T> end(std::vector<std::unique_ptr<T>>* c) { return RawVectorIterator<T>(c->end()); }


/* Template class for deque of unique_ptr with useful utility functions */
template <typename T>
class UniqueDeque : public std::deque<std::unique_ptr<T>>
{
public:
	void Add(std::unique_ptr<T> uptr, bool addTop = false)
	{
		if (addTop)
		{
			this->push_front(std::move(uptr));
		}
		else
		{
			this->push_back(std::move(uptr));
		}
	}

	std::unique_ptr<T> Remove(T* p)
	{
		std::unique_ptr<T> uptr;

		typename UniqueDeque::iterator it = Find(p);
		if (it != this->end())
		{
			uptr = std::move(*it);
			this->erase(it);
		}

		return uptr;
	}

	typename UniqueDeque::iterator Find(T* p)
	{
		return std::find_if(this->begin(), this->end(),
			[p](std::unique_ptr<T>& uptr)
			{
				return uptr.get() == p;
			});
	}

	T* Find(int id)
	{
		for (std::unique_ptr<T>& uptr : *this)
		{
			if (uptr->GetId() == id)
			{
				return uptr.get();
			}
		}

		return nullptr;
	}
};

#endif
