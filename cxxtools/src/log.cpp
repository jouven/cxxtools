/* log.cpp
   Copyright (C) 2003,2004 Tommi Maekitalo

This file is part of cxxtools.

Cxxtools is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Cxxtools is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with cxxtools; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330,
Boston, MA  02111-1307  USA
*/

#include <cxxtools/log/cxxtools_init.h>
#include <cxxtools/thread.h>
#include <cxxtools/udp.h>
#include <cxxtools/udpstream.h>
#include <list>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

namespace cxxtools
{
  class LoggerImpl : public Logger
  {
      static std::string fname;
      static std::ofstream outfile;
      static net::UdpSender loghost;
      static net::UdpOStream udpmessage;
      static unsigned maxfilesize;
      static unsigned maxbackupindex;

      static std::string mkfilename(unsigned idx);

    public:
      LoggerImpl(const std::string& c, log_level_type l)
        : Logger(c, l)
        { }
      std::ostream& getAppender() const;
      static void doRotate();
      static void setFile(const std::string& fname);
      static void setMaxFileSize(unsigned size)   { maxfilesize = size; }
      static void setMaxBackupIndex(unsigned idx) { maxbackupindex = idx; }
      static void setLoghost(const std::string& host, unsigned short int port);
  };

  std::ostream& LoggerImpl::getAppender() const
  {
    if (outfile.is_open())
    {
      if (maxfilesize > 0 && outfile.tellp() > maxfilesize)
        doRotate();
      outfile.clear();
      return outfile;
    }
    else if (loghost.isConnected())
      return udpmessage;
    else
      return std::cout;
  }

  std::string LoggerImpl::mkfilename(unsigned idx)
  {
    std::ostringstream f;
    f << fname << '.' << idx;
    return f.str();
  }

  void LoggerImpl::doRotate()
  {
    outfile.clear();
    outfile.close();

    // ignore unlink- and rename-errors. In case of failure the
    // original file is reopened

    std::string newfilename = mkfilename(maxbackupindex);
    ::unlink(newfilename.c_str());
    for (unsigned idx = maxbackupindex; idx > 0; --idx)
    {
      std::string oldfilename = mkfilename(idx - 1);
      ::rename(oldfilename.c_str(), newfilename.c_str());
      newfilename = oldfilename;
    }

    ::rename(fname.c_str(), newfilename.c_str());

    outfile.open(fname.c_str(), std::ios::out | std::ios::app);
  }

  std::string LoggerImpl::fname;
  std::ofstream LoggerImpl::outfile;
  net::UdpSender LoggerImpl::loghost;
  net::UdpOStream LoggerImpl::udpmessage(LoggerImpl::loghost);
  unsigned LoggerImpl::maxfilesize = 0;
  unsigned LoggerImpl::maxbackupindex = 0;

  void LoggerImpl::setFile(const std::string& fname_)
  {
    fname = fname_;
    outfile.clear();
    outfile.open(fname_.c_str(), std::ios::out | std::ios::app);
  }

  void LoggerImpl::setLoghost(const std::string& host, unsigned short int port)
  {
    loghost.connect(host.c_str(), port);
  }

  typedef std::list<Logger*> loggers_type;
  static loggers_type loggers;
  RWLock Logger::rwmutex;
  Mutex Logger::mutex;

  Logger::log_level_type Logger::std_level = LOG_LEVEL_ERROR;

  Logger* Logger::getLogger(const std::string& category)
  {
    // search existing Logger
    RdLock rdLock(rwmutex);

    loggers_type::iterator lower_bound_it = loggers.begin();
    while (lower_bound_it != loggers.end()
        && (*lower_bound_it)->getCategory() < category)
      ++lower_bound_it;

    if (lower_bound_it != loggers.end()
     && (*lower_bound_it)->getCategory() == category)
        return *lower_bound_it;

    // Logger not in list - change to write-lock
    rdLock.unlock();
    WrLock wrLock(rwmutex);

    // we have to do it again after gaining write-lock
    lower_bound_it = loggers.begin();
    while (lower_bound_it != loggers.end()
        && (*lower_bound_it)->getCategory() < category)
      ++lower_bound_it;

    if (lower_bound_it != loggers.end()
     && (*lower_bound_it)->getCategory() == category)
        return *lower_bound_it;

    // Logger still not in list, but we have a position to insert

    // search best-fit Logger
    std::string::size_type best_len = 0;
    log_level_type best_level = std_level;

    for (loggers_type::iterator it = loggers.begin();
         it != loggers.end(); ++it)
    {
      if ((*it)->getCategory().size() > best_len
        && (*it)->getCategory().size() < category.size()
        && category.at((*it)->getCategory().size()) == '.'
        && category.compare(0, (*it)->getCategory().size(), (*it)->getCategory()) == 0)
      {
        best_len = (*it)->getCategory().size();
        // update log-level
        best_level = (*it)->getLogLevel();
      }
    }

    // insert the new Logger in list and return pointer to the new list-element
    return *(loggers.insert(lower_bound_it, new LoggerImpl(category, best_level)));
  }

  Logger* Logger::setLevel(const std::string& category, log_level_type l)
  {
    WrLock lock(rwmutex);

    // search for existing Logger
    loggers_type::iterator it = loggers.begin();
    while (it != loggers.end()
        && (*it)->getCategory() < category)
      ++it;

    if (it == loggers.end() || (*it)->getCategory() != category)
    {
      // Logger not found - create new
      it = loggers.insert(it, new LoggerImpl(category, l));
    }
    else
      (*it)->setLogLevel(l); // Logger found - set level

    // return pointer to object in list
    return *it;
  }

  namespace
  {
    static char digits[] = "0123456789";

    inline char hiDigit(int i)
    { return digits[i / 10]; }

    inline char loDigit(int i)
    { return digits[i % 10]; }
  }


  std::ostream& Logger::logentry(const char* level)
  {
    struct timeval t;
    struct tm tt;
    gettimeofday(&t, 0);
    time_t sec = static_cast<time_t>(t.tv_sec);
    localtime_r(&sec, &tt);

    std::ostream& out = getAppender();
    out << 1900 + tt.tm_year << '-'
        << hiDigit(tt.tm_mon + 1) << loDigit(tt.tm_mon + 1) << '-'
        << hiDigit(tt.tm_mday) << loDigit(tt.tm_mday) << ' '
        << hiDigit(tt.tm_hour) << loDigit(tt.tm_hour) << ':'
        << hiDigit(tt.tm_min) << loDigit(tt.tm_min) << ':'
        << hiDigit(tt.tm_sec) << loDigit(tt.tm_sec) << '.'
        << loDigit(t.tv_usec / 100000)    << loDigit(t.tv_usec / 10000 % 10)
        << loDigit(t.tv_usec / 1000 % 10) << loDigit(t.tv_usec / 100 % 10)
        << loDigit(t.tv_usec / 10 % 10)
        << " [" << pthread_self() << "] "
        << level << ' '
        << category << " - ";

    return out;
  }

  LogTracer::~LogTracer()
  {
    if (msg)
    {
      if (l->isEnabled(Logger::LOG_LEVEL_TRACE))
      {
        cxxtools::MutexLock lock(cxxtools::Logger::mutex);
        l->logentry("TRACE")
          << "EXIT " << msg->str() << std::endl;
      }
      delete msg;
    }
  }

  std::ostream& LogTracer::logentry()
  {
    if (!msg)
      msg = new std::ostringstream();
    return *msg;
  }

  void LogTracer::enter()
  {
    if (msg && l->isEnabled(Logger::LOG_LEVEL_TRACE))
    {
      cxxtools::MutexLock lock(cxxtools::Logger::mutex);
      l->logentry("TRACE")
        << "ENTER " << msg->str() << std::endl;
    }
  }
}

void log_init(const std::string& propertyfilename)
{
  log_init(cxxtools::Logger::LOG_LEVEL_ERROR);

  std::ifstream in(propertyfilename.c_str());

  enum state_type {
    state_0,
    state_token,
    state_tokensp,
    state_category,
    state_level,
    state_rootlevel,
    state_filename0,
    state_filename,
    state_host0,
    state_host,
    state_port,
    state_fsize0,
    state_fsize,
    state_maxbackupindex0,
    state_maxbackupindex,
    state_skip
  };
  
  state_type state = state_0;

  char ch;
  std::string token;
  std::string category;
  std::string filename;
  std::string host;
  unsigned short int port;
  unsigned fsize;
  unsigned maxbackupindex;

  cxxtools::Logger::log_level_type level;
  while (in.get(ch))
  {
    switch (state)
    {
      case state_0:
        if (std::isalnum(ch) || ch == '_')
        {
          token = std::toupper(ch);
          state = state_token;
        }
        else if (!std::isspace(ch))
          state = state_skip;
        break;

      case state_token:
        if (ch == '.')
        {
          if (token == "LOGGER")
            state = state_category;
          else
          {
            token.clear();
            state = state_token;
          }
        }
        else if (ch == '=' && token == "ROOTLOGGER")
          state = state_rootlevel;
        else if (ch == '=' && token == "FILE")
          state = state_filename0;
        else if (ch == '=' && token == "HOST")
          state = state_host0;
        else if (ch == '=' && token == "MAXFILESIZE")
          state = state_fsize0;
        else if (ch == '=' && token == "MAXBACKUPINDEX")
          state = state_maxbackupindex0;
        else if (ch == '\n')
          state = state_0;
        else if (std::isspace(ch))
          state = state_tokensp;
        else if (std::isalnum(ch) || ch == '_')
          token += std::toupper(ch);
        else
        {
          token.clear();
          state = state_skip;
        }
        break;

      case state_tokensp:
        if (ch == '=' && token == "ROOTLOGGER")
          state = state_rootlevel;
        else if (ch == '=' && token == "FILE")
          state = state_filename0;
        else if (ch == '=' && token == "HOST")
          state = state_host0;
        else if (ch == '=' && token == "MAXFILESIZE")
          state = state_fsize0;
        else if (ch == '=' && token == "MAXBACKUPINDEX")
          state = state_maxbackupindex0;
        else if (ch == '\n')
          state = state_0;
        else if (!std::isspace(ch))
          state = state_skip;
        break;

      case state_category:
        if (std::isalnum(ch) || ch == '_' || ch == '.')
          category += ch;
        else if (ch == '=')
          state = state_level;
        else
        {
          category.clear();
          token.clear();
          state = (ch == '\n' ? state_0 : state_skip);
        }
        break;

      case state_level:
      case state_rootlevel:
        if (ch != '\n' && std::isspace(ch))
          break;

        switch (ch)
        {
          case 'F': level = cxxtools::Logger::LOG_LEVEL_FATAL; break;
          case 'E': level = cxxtools::Logger::LOG_LEVEL_ERROR; break;
          case 'W': level = cxxtools::Logger::LOG_LEVEL_WARN; break;
          case 'I': level = cxxtools::Logger::LOG_LEVEL_INFO; break;
          case 'D': level = cxxtools::Logger::LOG_LEVEL_DEBUG; break;
          case 'T': level = cxxtools::Logger::LOG_LEVEL_TRACE; break;
          default:  level = cxxtools::Logger::getStdLevel(); break;
        }
        if (state == state_rootlevel)
          cxxtools::Logger::setRootLevel(level);
        else
          cxxtools::Logger::setLevel(category, level);
        category.clear();
        token.clear();
        state = state_skip;
        break;

      case state_filename0:
        if (ch != '\n' && std::isspace(ch))
          break;

        state = state_filename;

      case state_filename:
        if (ch == '\n')
        {
          cxxtools::LoggerImpl::setFile(filename);
          token.clear();
          filename.clear();
          state = state_0;
        }
        else
          filename += ch;
        break;

      case state_host0:
        if (ch == '\n')
        {
          state = state_0;
          break;
        }
        else if (std::isspace(ch))
          break;

        state = state_host;

      case state_host:
        if (ch == ':')
        {
          port = 0;
          state = state_port;
        }
        else if (std::isspace(ch))
          state = state_skip;
        else
          host += ch;
        break;

      case state_port:
        if (std::isdigit(ch))
          port = port * 10 + ch - '0';
        else if (port > 0)
        {
          cxxtools::LoggerImpl::setLoghost(host, port);
          state = (ch == '\n' ? state_0 : state_skip);
        }
        break;

      case state_fsize0:
        if (ch == '\n')
        {
          state = state_0;
          break;
        }
        else if (std::isspace(ch))
          break;

        state = state_fsize;
        fsize = 0;

      case state_fsize:
        if (std::isdigit(ch))
          fsize = fsize * 10 + ch - '0';
        else if (ch == '\n')
        {
          cxxtools::LoggerImpl::setMaxFileSize(fsize);
          state = state_0;
        }
        else
        {
          if (ch == 'k' || ch == 'K')
            fsize *= 1024;
          else if (ch == 'M')
            fsize *= 1024 * 1024;

          cxxtools::LoggerImpl::setMaxFileSize(fsize);
          state = state_skip;
        }
        break;

      case state_maxbackupindex0:
        if (ch == '\n')
        {
          state = state_0;
          break;
        }
        else if (std::isspace(ch))
          break;

        state = state_maxbackupindex;
        maxbackupindex = 0;

      case state_maxbackupindex:
        if (std::isdigit(ch))
          maxbackupindex = maxbackupindex * 10 + ch - '0';
        else if (ch == '\n')
        {
          cxxtools::LoggerImpl::setMaxBackupIndex(maxbackupindex);
          state = state_0;
        }
        else
        {
          cxxtools::LoggerImpl::setMaxBackupIndex(maxbackupindex);
          state = state_skip;
        }
        break;

      case state_skip:
        if (ch == '\n')
          state = state_0;
        break;
    }
  }
}

void log_init()
{
  char* LOGPROPERTIES = ::getenv("LOGPROPERTIES");
  if (LOGPROPERTIES)
    log_init(LOGPROPERTIES);
  else
  {
    struct stat s;
    if (stat("log.properties", &s) == 0)
      log_init("log.properties");
    else
      log_init(cxxtools::Logger::LOG_LEVEL_ERROR);
  }
}

