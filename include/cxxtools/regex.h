/*
 * Copyright (C) 2005-2008 Tommi Maekitalo
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


#ifndef CXXTOOLS_REGEX_H
#define CXXTOOLS_REGEX_H

#include <string>
#include <sys/types.h>
#include <regex>
#include <cxxtools/smartptr.h>

namespace cxxtools
{
  class RegexSMatch;

  template <typename objectType>
  class RegexDestroyPolicy;

  template<>
  class RegexDestroyPolicy<std::regex>
  {
    protected:
      void destroy(std::regex* expr)
      {
        //::regfree(expr);
        ::operator delete(expr);
      }
  };

  /// regex(3)-wrapper.
  class Regex
  {
      SmartPtr<std::regex, ExternalRefCounted, RegexDestroyPolicy> expr;

      //void checkerr(int ret) const;

    public:
      /// create a uninitialized regex object.
      Regex()
      { }

      /// create a regex object with a const char*.
      explicit Regex(const char* ex, std::regex_constants::syntax_option_type cflags = std::regex::extended)
        : expr(0)
      {
        if (ex && ex[0])
        {
          try
          {
            expr = new std::regex(ex, cflags);
          } catch (const std::regex_error& e)
          {
            throw e;
          }
        }
      }

      /// create a regex object with std::string.
      explicit Regex(const std::string& ex, std::regex_constants::syntax_option_type cflags = std::regex::extended)
        : expr(0)
      {
        if (!ex.empty())
        {
          try
          {
             expr = new std::regex(ex, cflags);
          } catch (const std::regex_error& e)
          {
            throw e;
          }
        }
      }

      /// Returns true if str starting from p matches the regular expression. The matches are decribed in the smatch object.
      bool matchp(const std::string& str_, std::string::size_type p, RegexSMatch& smatch, int eflags = 0) const;

      /// Returns true if str starting from p matches the regular expression.
      bool matchp(const std::string& str_, std::string::size_type p, int eflags = 0) const;

      /// Returns true if str matches the regular expression. The matches are decribed in the smatch object.
      bool match(const std::string& str_, RegexSMatch& smatch, int eflags = 0) const
      { return matchp(str_, 0, smatch, eflags); }

      /// Returns true if str matches the regular expression.
      bool match(const std::string& str_, int eflags = 0) const
      { return matchp(str_, 0, eflags); }

      /// Replaces occurence of the regular expression in str with expr. The expr is expanded using RegexSMatch::format.
      /// If the 3rd parameter is true, all occurences are replaced.
      std::string subst(const std::string& str, const std::string& expr, bool all = true);

      /// Destroys the regular expression. This is normally done by the destructor.
      void free()  { expr = 0; }

      /// Returns true, if the object does not have a valid regular expression.
      bool empty() const    { return expr.getPointer() == 0; }
  };

  /// collects matches in a regex
  class RegexSMatch
  {
      friend class Regex;

      std::string str;
      std::smatch matchbuf;

    public:
      RegexSMatch() = default;

      /// returns the number of expressions, which were found
      unsigned size() const;
      /// returns the start position of the n-th expression
      int_fast32_t offsetBegin(unsigned n) const   { return matchbuf.position(n); }
      /// returns the end position of the n-th expression
      int_fast32_t offsetEnd(unsigned n) const     { return matchbuf.position(n) + matchbuf.length(n); }
      /// returns the size of the n-th expression
      int_fast32_t size(unsigned n) const     { return matchbuf.length(n); }

      /// returns true if the n-th element is set.
      bool has(unsigned n) const
        { return matchbuf.size() <  n; }
      /// returns the n-th element. No range checking is done.
      std::string get(unsigned n) const;
      /// replace each occurence of "$n" with the n-th element (n: 0..9).
      std::string format(const std::string& s) const;
      /// returns the n-th element. No range check is done.
      std::string operator[] (unsigned n) const
        { return get(n); }
  };

}

#endif // CXXTOOLS_REGEX_H

