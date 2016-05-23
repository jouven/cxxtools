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
#ifndef CXXTOOLS_LIBRARYIMPL_H
#define CXXTOOLS_LIBRARYIMPL_H

#include "cxxtools/systemerror.h"
#include "cxxtools/refcounted.h"
#include <string>
#ifdef __MINGW32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace cxxtools {

class LibraryImpl : public RefCounted
{
    public:
        LibraryImpl()
        : RefCounted(1)
        , _handle(0)
        { }

        LibraryImpl(const std::string& path)
        : RefCounted(1)
        , _handle(0)
        {
            this->open(path);
        }

        ~LibraryImpl()
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

        void open(const std::string& path);

        void close();

        void* resolve(const char* symbol) const;

        bool failed()
        { return _handle == 0; }

        static std::string suffix()
        {
            return ".so";
        }

        static std::string prefix()
        {
            return "lib";
        }

    private:
        #ifdef __MINGW32__
        HMODULE _handle;
        #else
        void* _handle;
        #endif
};

} // namespace cxxtools

#endif
