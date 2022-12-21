// file      : libcutl/compiler/code-stream.txx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    // code_stream
    //

    template <typename C>
    code_stream<C>::~code_stream ()
    {
    }

    // from_streambuf_adapter
    //

    template <typename C>
    void from_streambuf_adapter<C>::
    put (C c)
    {
      int_type i (stream_.sputc (c));

      if (i == traits_type::eof ())
        throw eof ();
    }

    template <typename C>
    void from_streambuf_adapter<C>::
    unbuffer ()
    {
      if (stream_.pubsync () != 0)
        throw sync ();
    }

    // to_streambuf_adapter
    //

    template <typename C>
    typename to_streambuf_adapter<C>::int_type to_streambuf_adapter<C>::
    overflow (int_type i)
    {
      try
      {
        stream_.put (traits_type::to_char_type (i));
        return i;
      }
      catch (typename from_streambuf_adapter<C>::eof const&)
      {
        return traits_type::eof ();
      }
    }

    template <typename C>
    int to_streambuf_adapter<C>::
    sync ()
    {
      return 0;
    }

    // ostream_filter
    //

    template <template <typename> class S, typename C>
    ostream_filter<S, C>::
    ostream_filter (std::basic_ostream<C>& os)
        : os_ (os),
          prev_ (os_.rdbuf ()),
          from_adapter_ (*prev_),
          stream_ (from_adapter_),
          to_adapter_ (stream_)
    {
      os_.rdbuf (&to_adapter_);
    }

    template <template <typename> class S, typename C>
    ostream_filter<S, C>::
    ~ostream_filter ()
    {
      try
      {
        stream_.unbuffer ();
      }
      catch (...)
      {
        os_.rdbuf (prev_);
      }

      os_.rdbuf (prev_);
    }
  }
}
