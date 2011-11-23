/*
 * Copyright (C) 2011 Tommi Maekitalo
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include <cxxtools/jsondeserializer.h>

namespace cxxtools
{
    JsonDeserializer::JsonDeserializer(std::istream& in, TextCodec<Char, char>* codec)
        : _ts(new TextIStream(in, codec)),
          _in(*_ts)
    { }

    JsonDeserializer::JsonDeserializer(TextIStream& in)
        : _ts(0),
          _in(in)
    { }

    JsonDeserializer::~JsonDeserializer()
    {
        delete _ts;
    }

    void JsonDeserializer::get(IDeserializer* deser)
    {
        _parser.begin(*deser, _context);
        Char ch;
        int ret;
        while (_in.get(ch))
        {
            ret = _parser.advance(ch);
            if (ret == -1)
                _in.putback(ch);
            if (ret != 0)
                return;
        }
        _parser.finish();
      }
}
