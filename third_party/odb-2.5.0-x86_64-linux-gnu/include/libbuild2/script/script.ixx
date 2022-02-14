// file      : libbuild2/script/script.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace build2
{
  namespace script
  {
    inline command_to_stream
    operator&= (command_to_stream& x, command_to_stream y)
    {
      return x = static_cast<command_to_stream> (
        static_cast<uint16_t> (x) & static_cast<uint16_t> (y));
    }

    inline command_to_stream
    operator|= (command_to_stream& x, command_to_stream y)
    {
      return x = static_cast<command_to_stream> (
        static_cast<uint16_t> (x) | static_cast<uint16_t> (y));
    }

    inline command_to_stream
    operator& (command_to_stream x, command_to_stream y) {return x &= y;}

    inline command_to_stream
    operator| (command_to_stream x, command_to_stream y) {return x |= y;}


    // command
    //
    inline ostream&
    operator<< (ostream& o, const command& c)
    {
      to_stream (o, c, command_to_stream::all);
      return o;
    }

    // command_pipe
    //
    inline ostream&
    operator<< (ostream& o, const command_pipe& p)
    {
      to_stream (o, p, command_to_stream::all);
      return o;
    }

    // command_expr
    //
    inline ostream&
    operator<< (ostream& o, const command_expr& e)
    {
      to_stream (o, e, command_to_stream::all);
      return o;
    }
  }
}
