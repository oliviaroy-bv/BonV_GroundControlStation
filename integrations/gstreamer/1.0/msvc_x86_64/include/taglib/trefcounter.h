/***************************************************************************
    copyright            : (C) 2013 by Tsuda Kageyu
    email                : tsuda.kageyu@gmail.com
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#ifndef TAGLIB_REFCOUNTER_H
#define TAGLIB_REFCOUNTER_H

#include "taglib_export.h"
#include "taglib.h"

#if defined(HAVE_GCC_ATOMIC)
# define ATOMIC_INT int
# define ATOMIC_INC(x) __sync_add_and_fetch(&(x), 1)
# define ATOMIC_DEC(x) __sync_sub_and_fetch(&(x), 1)
#elif defined(HAVE_WIN_ATOMIC)
# if !defined(NOMINMAX)
#   define NOMINMAX
# endif
# include <windows.h>
# define ATOMIC_INT long
# define ATOMIC_INC(x) InterlockedIncrement(&x)
# define ATOMIC_DEC(x) InterlockedDecrement(&x)
#elif defined(HAVE_MAC_ATOMIC)
# if defined(MAC_OS_VERSION_10_12) && MAC_OS_X_VERSION_MIN_ALLOWED >= MAC_OS_VERSION_10_12
#   include <stdatomic.h>
#   define ATOMIC_INT atomic_int
#   define ATOMIC_INC(x) std::atomic_fetch_add(&x, 1)
#   define ATOMIC_DEC(x) std::atomic_fetch_sub(&x, 1)
# else
#   include <libkern/OSAtomic.h>
#   define ATOMIC_INT int32_t
#   define ATOMIC_INC(x) OSAtomicIncrement32Barrier(&x)
#   define ATOMIC_DEC(x) OSAtomicDecrement32Barrier(&x)
# endif
#elif defined(HAVE_IA64_ATOMIC)
# include <ia64intrin.h>
# define ATOMIC_INT int
# define ATOMIC_INC(x) __sync_add_and_fetch(&x, 1)
# define ATOMIC_DEC(x) __sync_sub_and_fetch(&x, 1)
#else
# define ATOMIC_INT unsigned int
# define ATOMIC_INC(x) (++x)
# define ATOMIC_DEC(x) (--x)
#endif

#ifndef DO_NOT_DOCUMENT // Tell Doxygen to skip this class.
/*!
  * \internal
  * This is just used as a base class for shared classes in TagLib.
  *
  * \warning This <b>is not</b> part of the TagLib public API!
  */
namespace TagLib
{

  class TAGLIB_EXPORT RefCounter
  {
  public:
    RefCounter();
    virtual ~RefCounter();

    void ref();
    bool deref();
    int count() const;

  private:
    class RefCounterPrivate;
    RefCounterPrivate *d;
  };

  // BIC this old class is needed by tlist.tcc and tmap.tcc
  class RefCounterOld
  {
  public:
    RefCounterOld() : refCount(1) {}

    void ref() { ATOMIC_INC(refCount); }
    bool deref() { return ATOMIC_DEC(refCount); }
    int32_t count() { return refCount; }
  private:
    volatile ATOMIC_INT refCount;
  };

}

#endif // DO_NOT_DOCUMENT
#endif

