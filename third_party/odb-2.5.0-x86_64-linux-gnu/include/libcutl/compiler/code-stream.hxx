// file      : libcutl/compiler/code-stream.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_COMPILER_CODE_STREAM_HXX
#define LIBCUTL_COMPILER_CODE_STREAM_HXX

#include <ostream>

#include <libcutl/exception.hxx>

namespace cutl
{
  namespace compiler
  {
    //
    //
    template <typename C>
    class code_stream
    {
    public:
      code_stream () {}

      virtual
      ~code_stream ();

    public:
      virtual void
      put (C) = 0;

      // Unbuffer flushes internal formatting buffers (if any).
      // Note that unbuffer is not exactly flushing since it can
      // result in formatting errors and in general can not be
      // called at arbitrary points. Natural use case would be
      // to call unbuffer at the end of the stream when no more
      // data is expected.
      //
      virtual void
      unbuffer () = 0;

    private:
      code_stream (code_stream const&);

      code_stream&
      operator= (code_stream const&);
    };

    //
    //
    template <typename C>
    class from_streambuf_adapter: public code_stream<C>
    {
    public:
      typedef typename std::basic_streambuf<C>::traits_type traits_type;
      typedef typename std::basic_streambuf<C>::int_type int_type;

      class eof: exception {};
      class sync: exception {};

    public:
      from_streambuf_adapter (std::basic_streambuf<C>& stream)
          : stream_ (stream)
      {
      }

    private:
      from_streambuf_adapter (from_streambuf_adapter const&);

      from_streambuf_adapter&
      operator= (from_streambuf_adapter const&);

    public:
      virtual void
      put (C c);

      virtual void
      unbuffer ();

    private:
      std::basic_streambuf<C>& stream_;
    };

    //
    //
    template <typename C>
    class to_streambuf_adapter: public std::basic_streambuf<C>
    {
    public:
      typedef typename std::basic_streambuf<C>::traits_type traits_type;
      typedef typename std::basic_streambuf<C>::int_type int_type;

    public:
      to_streambuf_adapter (code_stream<C>& stream)
          : stream_ (stream)
      {
      }

    private:
      to_streambuf_adapter (to_streambuf_adapter const&);

      to_streambuf_adapter&
      operator= (to_streambuf_adapter const&);

    public:
      virtual int_type
      overflow (int_type i);

      // Does nothing since calling unbuffer here would be dangerous.
      // See the note in code_stream.
      //
      virtual int
      sync ();

    private:
      code_stream<C>& stream_;
    };

    //
    //
    template <template <typename> class S, typename C>
    class ostream_filter
    {
    public:
      typedef S<C> stream_type;

      ostream_filter (std::basic_ostream<C>& os);
      ~ostream_filter () /*noexcept (false)*/;

      stream_type&
      stream ()
      {
        return stream_;
      }

    private:
      ostream_filter (ostream_filter const&);

      ostream_filter&
      operator= (ostream_filter const&);

    private:
      std::basic_ostream<C>& os_;
      std::basic_streambuf<C>* prev_;
      from_streambuf_adapter<C> from_adapter_;
      stream_type stream_;
      to_streambuf_adapter<C> to_adapter_;
    };
  }
}

#include <libcutl/compiler/code-stream.txx>

#endif // LIBCUTL_COMPILER_CODE_STREAM_HXX
