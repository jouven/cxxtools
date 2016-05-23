/*
 * Copyright (C) 2006-2008 Marc Boris Duerner
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "libraryimpl.h"
#ifdef __MINGW32__
#include <exception>
#define RTLD_LAZY 1

#define RTLD_NOW 2

#define RTLD_GLOBAL 4
#endif

namespace cxxtools {

void LibraryImpl::open(const std::string& path)
{
    if(_handle)
        return;

    /* RTLD_NOW: since lazy loading is not supported by every target platform
        RTLD_GLOBAL: make the external symbols in the loaded library available for subsequent libraries.
                    see also http://gcc.gnu.org/faq.html#dso
    */
    int flags = RTLD_NOW | RTLD_GLOBAL;

    #ifdef __MINGW32__
    _handle = ::LoadLibrary(path.c_str());
    #else
    _handle = ::dlopen(path.c_str(), flags);
    #endif
    if( !_handle )
    {
        #ifdef __MINGW32__
        throw std::runtime_error("Mingw porting, Not implemented yet.");
        #else
        throw OpenLibraryFailed(dlerror());
        #endif
    }
}


void LibraryImpl::close()
{
    if(_handle)
    {
       #ifdef __MINGW32__
       FreeLibrary(_handle);
       #else
       ::dlclose(_handle);
       #endif
    }
}


void* LibraryImpl::resolve(const char* symbol) const
{
    if(_handle)
    {
        #ifdef __MINGW32__
        return (void *)GetProcAddress(_handle, symbol);
        #else
        return ::dlsym(_handle, symbol);
        #endif
    }

    return 0;
}

} // namespace cxxtools
