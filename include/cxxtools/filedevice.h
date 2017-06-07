/*
 * Copyright (C) 2006-2007 Laurentiu-Gheorghe Crisan
 * Copyright (C) 2006-2007 Marc Boris Duerner
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
#ifndef cxxtools_System_FileDevice_h
#define cxxtools_System_FileDevice_h

#include <cxxtools/iodevice.h>

namespace cxxtools
{

class FileDevice : public IODevice 
{
    friend class FileDeviceImpl;

    private:
        class FileDeviceImpl* _impl;

    public:
        FileDevice();

        FileDevice( const std::string& path, IODevice::OpenMode mode, bool inherit = true);

        ~FileDevice();

        void open( const std::string& path, IODevice::OpenMode mode, bool inherit = true);

        const char* path() const
        { return _path.c_str(); }

        size_t size() const;

    protected:
        bool onSeekable() const
        { return true; }

        pos_type onSeek(off_type offset, std::ios::seekdir sd);

        size_t onPeek(char* buffer, size_t count);

        void onSync() const;

        IODeviceImpl& ioimpl();

        SelectableImpl& simpl();

    private:
        std::string _path;
};

} // namespace cxxtools

#endif
