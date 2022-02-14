// file      : libbuild2/utility.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace build2
{
  template <typename I, typename F>
  void
  append_option_values (cstrings& args, const char* o, I b, I e, F&& get)
  {
    if (b != e)
    {
      args.reserve (args.size () + (e - b));

      for (; b != e; ++b)
      {
        args.push_back (o);
        args.push_back (get (*b));
      }
    }
  }

  template <typename I, typename F>
  void
  append_option_values (sha256& cs, const char* o, I b, I e, F&& get)
  {
    for (; b != e; ++b)
    {
      cs.append (o);
      cs.append (get (*b));
    }
  }

  template <typename K>
  basic_path<char, K>
  relative (const basic_path<char, K>& p)
  {
    using path = basic_path<char, K>;

    const dir_path& b (*relative_base);

    if (p.simple () || b.empty ())
      return p;

    if (p.sub (b))
      return p.leaf (b);

    if (p.root_directory () == b.root_directory ())
    {
      path r (p.relative (b));

      if (r.string ().size () < p.string ().size ())
        return r;
    }

    return p;
  }

  [[noreturn]] LIBBUILD2_SYMEXPORT void
  run_io_error (const char*[], const io_error&);

  template <typename T, typename F>
  T
  run (uint16_t verbosity,
       const process_env& pe,
       const char* args[],
       F&& f,
       bool err,
       bool ignore_exit,
       sha256* checksum)
  {
    process pr (run_start (verbosity,
                           pe,
                           args,
                           0  /* stdin */,
                           -1 /* stdout */,
                           err));
    T r;
    string l; // Last line of output.

    try
    {
      ifdstream is (move (pr.in_ofd), butl::fdstream_mode::skip);

      // Make sure we keep the last line.
      //
      for (bool last (is.peek () == ifdstream::traits_type::eof ());
           !last && getline (is, l); )
      {
        last = (is.peek () == ifdstream::traits_type::eof ());

        trim (l);

        if (checksum != nullptr)
          checksum->append (l);

        if (r.empty ())
        {
          r = f (l, last);

          if (!r.empty () && checksum == nullptr)
            break;
        }
      }

      is.close ();
    }
    catch (const io_error& e)
    {
      if (run_wait (args, pr))
        run_io_error (args, e);

      // If the child process has failed then assume the io error was
      // caused by that and let run_finish() deal with it.
    }

    if (!(run_finish_impl (args, pr, err, l) || ignore_exit))
      r = T ();

    return r;
  }
}
