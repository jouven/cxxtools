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
#include "iodeviceimpl.h"
#include "cxxtools/ioerror.h"
#include "error.h"
#include <cerrno>
#include <cassert>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#ifdef __MINGW32__
#include "fcntlMingw.h"
#include "fsync.h"
#define WIN32_LEAN_AND_MEAN
#include <mswsock.h>
#else
#include <sys/poll.h>
#endif

#include <cxxtools/log.h>
#include <cxxtools/hdstream.h>

log_define("cxxtools.iodevice.impl")

namespace cxxtools {

const short IODeviceImpl::POLLERR_MASK= POLLERR | POLLHUP | POLLNVAL;
const short IODeviceImpl::POLLIN_MASK= POLLIN;
const short IODeviceImpl::POLLOUT_MASK= POLLOUT;

IODeviceImpl::IODeviceImpl(IODevice& device)
: _device(device)
, _fd(-1)
, _timeout(Selectable::WaitInfinite)
, _pfd(0)
, _sentry(0)
, _errorPending(false)
{ }



IODeviceImpl::~IODeviceImpl()
{
    assert(_pfd == 0);

    if(_sentry)
        _sentry->detach();
}


void IODeviceImpl::open(const std::string& path, IODevice::OpenMode mode, bool inherit)
{
    int flags = O_RDONLY;

    if( (mode & IODevice::Read ) && (mode & IODevice::Write) )
    {
        flags |= O_RDWR;
    }
    else if(mode & IODevice::Write)
    {
        flags |= O_WRONLY;
    }
    else if(mode & IODevice::Read  )
    {
        flags |= O_RDONLY;
    }
    #ifdef __MINGW32__
    if(mode & IODevice::Async)
       flags |= FIONBIO;
    #else
    if(mode & IODevice::Async)
        flags |= O_NONBLOCK;
    #endif
    if(mode & IODevice::Trunc)
        flags |= O_TRUNC;

    #ifdef __MINGW32__
    flags |= 0;
    #else
    flags |=  O_NOCTTY;
    #endif

    _fd = ::open( path.c_str(), flags );
    if(_fd == -1)
        throw AccessFailed(getErrnoString("open failed"));

    if (!inherit)
    {
        int flags = fcntl(_fd, F_GETFD);
        flags |= FD_CLOEXEC ;
        int ret = fcntl(_fd, F_SETFD, flags);
        if(-1 == ret)
            throw IOError(getErrnoString("Could not set FD_CLOEXEC"));
    }

}


void IODeviceImpl::open(int fd, bool isAsync, bool inherit)
{
    _fd = fd;
    #ifndef __MINGW32__
    if (isAsync)
    {
        int flags = fcntl(_fd, F_GETFL);
        flags |= O_NONBLOCK ;
        int ret = fcntl(_fd, F_SETFL, flags);
        if(-1 == ret)
            throw IOError(getErrnoString("Could not set fd to non-blocking"));
    }
    #endif
    if (!inherit)
    {
        int flags = fcntl(_fd, F_GETFD);
        flags |= FD_CLOEXEC ;
        int ret = fcntl(_fd, F_SETFD, flags);
        if(-1 == ret)
            throw IOError(getErrnoString("Could not set FD_CLOEXEC"));
    }

}


void IODeviceImpl::close()
{
    log_debug("close device; fd=" << _fd << " pfd=" << _pfd);

    if(_fd != -1)
    {
        int fd = _fd;
        _fd = -1;
        _pfd = 0;

        while ( ::close(fd) != 0 )
        {
            if( errno != EINTR )
            {
                log_error("close of iodevice failed; errno=" << errno);
                throw IOError(getErrnoString("Could not close file handle"));
            }
        }
    }
}


size_t IODeviceImpl::beginRead(char* /*buffer*/, size_t /*n*/, bool& /*eof*/)
{
    if(_pfd)
    {
        _pfd->events |= POLLIN;
    }

    return 0;
}


size_t IODeviceImpl::endRead(bool& eof)
{
    if(_pfd)
    {
        _pfd->events &= ~POLLIN;
    }

    if (_errorPending)
    {
        _errorPending = false;
        throw IOError("read error");
    }

    return this->read( _device.rbuf(), _device.rbuflen(), eof );
}


size_t IODeviceImpl::read( char* buffer, size_t count, bool& eof )
{
    ssize_t ret = 0;

    while(true)
    {
        ret = ::read( _fd, (void*)buffer, count);

        if(ret > 0)
        {
            log_debug("::read(" << _fd << ", " << count << ") returned " << ret << " => \"" << hexDump(buffer, ret) << '"');
            break;
        }

        log_debug("::read(" << _fd << ", " << count << ") returned " << ret << " errno=" << errno);

        if(ret == 0 || errno == ECONNRESET)
        {
            eof = true;
            return 0;
        }

        if(errno == EINTR)
            continue;

        if(errno != EAGAIN)
            throw IOError(getErrnoString("read failed"));

        pollfd pfd;
        pfd.fd = this->fd();
        pfd.revents = 0;
        pfd.events = POLLIN;

        bool ret = this->wait(_timeout, pfd);
        if(false == ret)
        {
            log_debug("timeout (" << _timeout << ')');
            throw IOTimeout();
        }
    }

    return ret;
}


size_t IODeviceImpl::beginWrite(const char* buffer, size_t n)
{
    log_debug("::write(" << _fd << ", \"" << hexDump(buffer, n) << "\", " << n << ')');
    ssize_t ret = ::write(_fd, (const void*)buffer, n);

    log_debug("write returned " << ret);
    if (ret > 0)
        return static_cast<size_t>(ret);

    if (ret == 0 || errno == ECONNRESET || errno == EPIPE)
        throw IOError("lost connection to peer");

    if(_pfd)
    {
        _pfd->events |= POLLOUT;
    }

    return 0;
}


size_t IODeviceImpl::endWrite()
{
    if(_pfd)
    {
        _pfd->events &= ~POLLOUT;
    }

    if (_errorPending)
    {
        _errorPending = false;
        throw IOError("write error");
    }

    if (_device.wavail() > 0)
    {
        log_debug("write pending " << _device.wavail());
        size_t n = _device.wavail();
        return n;
    }

    return this->write( _device.wbuf(), _device.wbuflen() );
}


size_t IODeviceImpl::write( const char* buffer, size_t count )
{
    ssize_t ret = 0;

    while(true)
    {
        log_debug("::write(" << _fd << ", buffer, " << count << ')');

        ret = ::write(_fd, (const void*)buffer, count);
        log_debug("write returned " << ret);
        if(ret > 0)
            break;

        if(ret == 0 || errno == ECONNRESET || errno == EPIPE)
            throw IOError("lost connection to peer");

        if(errno == EINTR)
            continue;

        if(errno != EAGAIN)
            throw IOError(getErrnoString("Could not write to file handle"));

        pollfd pfd;
        pfd.fd = this->fd();
        pfd.revents = 0;
        pfd.events = POLLOUT;

        if (!this->wait(_timeout, pfd))
        {
            throw IOTimeout();
        }
    }

    return static_cast<size_t>(ret);
}


void IODeviceImpl::sigwrite(int sig)
{
    ::write(_fd, (const void*)&sig, sizeof(sig));
}


void IODeviceImpl::cancel()
{
    if(_pfd)
    {
        _pfd->events &= ~(POLLIN|POLLOUT);
    }
}


void IODeviceImpl::sync() const
{
    #ifdef __MINGW32__
    int ret = fsync(_fd);
    #else
    int ret = fsync(_fd);
    #endif
    if(ret != 0)
        throw IOError(getErrnoString("Could not sync handle"));
}


void IODeviceImpl::attach(SelectorBase& /*s*/)
{

}


void IODeviceImpl::detach(SelectorBase& /*s*/)
{
    _pfd = 0;
}


bool IODeviceImpl::wait(Timespan timeout)
{
    if (_device.wavail() > 0)
    {
        _device.outputReady(_device);
        return true;
    }

    pollfd pfd;
    this->initWait(pfd);
    this->wait(timeout, pfd);
    return this->checkPollEvent(pfd);
}


bool IODeviceImpl::wait(Timespan timeout, pollfd& pfd)
{
    int msecs = timeout < Timespan(0) ? -1
              : Milliseconds(timeout) > std::numeric_limits<int>::max()
                            ? std::numeric_limits<int>::max()
              : int(Milliseconds(timeout).ceil());

    log_debug("wait " << msecs << "ms");
    int ret = -1;
    do
    {
        #ifdef __MINGW32__
        ret = WSAPoll(&pfd, 1, msecs);
        #else
        ret = ::poll(&pfd, 1, msecs);
        #endif
    } while (ret == -1 && errno == EINTR);

    if (ret == -1)
        throw IOError(getErrnoString("poll failed"));

    return ret > 0;
}


void IODeviceImpl::initWait(pollfd& pfd)
{
    pfd.fd = this->fd();
    pfd.revents = 0;
    pfd.events = 0;

    if( _device.reading() )
        pfd.events |= POLLIN;
    if( _device.writing() )
        pfd.events |= POLLOUT;
}


size_t IODeviceImpl::initializePoll(pollfd* pfd, size_t pollSize)
{
    assert(pfd != 0);
    assert(pollSize >= 1);

    this->initWait(*pfd);
    _pfd = pfd;

    return 1;
}


bool IODeviceImpl::checkPollEvent()
{
    // _pfd can be 0 if the device is just added during wait iteration
    if (_pfd == 0)
        return false;

    return this->checkPollEvent(*_pfd);
}


bool IODeviceImpl::checkPollEvent(pollfd& pfd)
{
    log_trace("checkPollEvent");

    bool avail = false;

    DestructionSentry sentry(_sentry);

    if (pfd.revents & POLLERR_MASK)
    {
        _errorPending = true;

        try
        {
            bool reading = _device.reading();
            bool writing = _device.writing();

            if (reading)
            {
                avail = true;
                _device.inputReady(_device);
            }

            if( ! _sentry )
                return avail;

            if (writing)
            {
                avail = true;
                _device.outputReady(_device);
            }

            if( ! _sentry )
                return avail;

            if (!reading && !writing)
            {
                avail = true;
                _device.close();
            }
        }
        catch (...)
        {
            _errorPending = false;
            throw;
        }
        _errorPending = false;

        return avail;
    }

    if( _device.wavail() > 0 || (pfd.revents & POLLOUT_MASK) )
    {
        log_debug("send signal outputReady");
        _device.outputReady(_device);
        avail = true;
    }

    if( ! _sentry )
        return avail;

    if( pfd.revents & POLLIN_MASK )
    {
        log_debug("send signal inputReady");
        _device.inputReady(_device);
        avail = true;
    }

    return avail;
}

}
