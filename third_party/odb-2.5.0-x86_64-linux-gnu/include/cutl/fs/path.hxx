// file      : cutl/fs/path.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_FS_PATH_HXX
#define CUTL_FS_PATH_HXX

#include <string>
#include <ostream>

#include <cutl/exception.hxx>

#include <cutl/details/config.hxx>
#include <cutl/details/export.hxx>

namespace cutl
{
  namespace fs
  {
    template <typename C>
    class basic_path;

    template <typename C>
    struct path_traits
    {
      typedef std::basic_string<C> string_type;
      typedef typename string_type::size_type size_type;

      // Canonical directory and path seperators.
      //
#ifdef _WIN32
      static C const directory_separator = '\\';
      static C const path_separator = ';';
#else
      static C const directory_separator = '/';
      static C const path_separator = ':';
#endif

      // Directory separator tests. On some platforms there
      // could be multiple seperators. For example, on Windows
      // we check for both '/' and '\'.
      //
      static bool
      is_separator (C c)
      {
#ifdef _WIN32
        return c == '\\' || c == '/';
#else
        return c == '/';
#endif
      }

      static size_type
      find_separator (string_type const& s, size_type pos = 0)
      {
        for (size_type n (s.size ()); pos < n; ++pos)
        {
          if (is_separator (s[pos]))
            return pos;
        }

        return string_type::npos;
      }

      static size_type
      rfind_separator (string_type const& s, size_type pos = string_type::npos)
      {
        if (pos == string_type::npos)
          pos = s.size ();
        else
          pos++;

        for (; pos > 0; --pos)
        {
          if (is_separator (s[pos - 1]))
            return pos - 1;
        }

        return string_type::npos;
      }

      static int
      compare (string_type const& l, string_type const& r)
      {
        size_type ln (l.size ()), rn (r.size ()), n (ln < rn ? ln : rn);
        for (size_type i (0); i != n; ++i)
        {
#ifdef _WIN32
          C lc (tolower (l[i])), rc (tolower (r[i]));
#else
          C lc (l[i]), rc (r[i]);
#endif
          if (is_separator (lc) && is_separator (rc))
            continue;

          if (lc < rc) return -1;
          if (lc > rc) return 1;
        }

        return ln < rn ? -1 : (ln > rn ? 1 : 0);
      }

    private:
#ifdef _WIN32
      static C
      tolower (C);
#endif
    };

    template <typename C>
    class invalid_basic_path;

    typedef basic_path<char> path;
    typedef invalid_basic_path<char> invalid_path;

    typedef basic_path<wchar_t> wpath;
    typedef invalid_basic_path<wchar_t> invalid_wpath;

    //
    //
    class LIBCUTL_EXPORT invalid_path_base: exception
    {
    public:
      virtual char const*
      what () const LIBCUTL_NOTHROW_NOEXCEPT;
    };

    template <typename C>
    class invalid_basic_path: public invalid_path_base
    {
    public:
      typedef std::basic_string<C> string_type;

      invalid_basic_path (C const* p): path_ (p) {}
      invalid_basic_path (string_type const& p): path_ (p) {}
      ~invalid_basic_path () LIBCUTL_NOTHROW_NOEXCEPT {}

      string_type const&
      path () const
      {
        return path_;
      }

    private:
      string_type path_;
    };

    template <typename C>
    class basic_path
    {
    public:
      typedef std::basic_string<C> string_type;
      typedef typename string_type::size_type size_type;

      typedef path_traits<C> traits;

      // Construct special empty path.
      //
      basic_path ()
      {
      }

      explicit
      basic_path (C const* s)
          : path_ (s)
      {
        init ();
      }

      basic_path (C const* s, size_type n)
          : path_ (s, n)
      {
        init ();
      }

      explicit
      basic_path (string_type const& s)
          : path_ (s)
      {
        init ();
      }

      void
      swap (basic_path& p)
      {
        path_.swap (p.path_);
      }

      void
      clear ()
      {
        path_.clear ();
      }

      static basic_path
      current ();

      static void
      current (basic_path const&);

    public:
      bool
      empty () const
      {
        return path_.empty ();
      }

      bool
      absolute () const;

      bool
      relative () const
      {
        return !absolute ();
      }

      bool
      root () const;

    public:
      // Return the path without the directory part.
      //
      basic_path
      leaf () const;

      // Return the directory part of the path or empty path if
      // there is no directory.
      //
      basic_path
      directory () const;

      // Return the path without the extension, if any.
      //
      basic_path
      base () const;

    public:
      // Normalize the path. This includes collapsing the '.' and '..'
      // directories if possible, collapsing multiple directory
      // separators, and converting all directory separators to the
      // canonical form. Returns *this.
      //
      basic_path&
      normalize ();

      // Make the path absolute using the current directory unless
      // it is already absolute.
      //
      basic_path&
      complete ();

    public:
      basic_path
      operator/ (basic_path const& x) const
      {
        basic_path r (*this);
        r /= x;
        return r;
      }

      basic_path&
      operator/= (basic_path const&);

      basic_path
      operator+ (string_type const& s) const
      {
        return basic_path (path_ + s);
      }

      basic_path&
      operator+= (string_type const& s)
      {
        path_ += s;
        return *this;
      }

      // Note that comparison is case-insensitive if the filesystem is
      // not case-sensitive (e.g., Windows).
      //
      bool
      operator== (basic_path const& x) const
      {
        return traits::compare (path_, x.path_) == 0;
      }

      bool
      operator!= (basic_path const& x) const
      {
        return !(*this == x);
      }

      bool
      operator< (basic_path const& x) const
      {
        return traits::compare (path_, x.path_) < 0;
      }

    public:
      const string_type&
      string () const
      {
        return path_;
      }

      // If possible, return a POSIX representation of the path. For example,
      // for a Windows path in the form foo\bar this function will return
      // foo/bar. If it is not possible to create a POSIX representation for
      // this path (e.g., c:\foo), this function will throw the invalid_path
      // exception.
      //
      string_type
      posix_string () const;

    private:
      void
      init ();

    private:
      string_type path_;
    };

    template <typename C>
    inline std::basic_ostream<C>&
    operator<< (std::basic_ostream<C>& os, basic_path<C> const& p)
    {
      return os << p.string ();
    }
  }
}

#include <cutl/fs/path.ixx>
#include <cutl/fs/path.txx>

#endif // CUTL_FS_PATH_HXX
