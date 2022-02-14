// file      : cutl/fs/path.ixx
// license   : MIT; see accompanying LICENSE file

#ifdef _WIN32
#  include <cctype>  // std::tolower
#  include <cwctype> // std::towlower
#endif

namespace cutl
{
  namespace fs
  {
#ifdef _WIN32
    template <>
    inline char path_traits<char>::
    tolower (char c)
    {
      return std::tolower (c);
    }

    template <>
    inline wchar_t path_traits<wchar_t>::
    tolower (wchar_t c)
    {
      return std::towlower (c);
    }
#endif

    template <typename C>
    inline bool basic_path<C>::
    absolute () const
    {
#ifdef _WIN32
      return path_.size () > 1 && path_[1] == ':';
#else
      return !path_.empty () && traits::is_separator (path_[0]);
#endif
    }

    template <typename C>
    inline bool basic_path<C>::
    root () const
    {
#ifdef _WIN32
      return path_.size () == 2 && path_[1] == ':';
#else
      return path_.size () == 1 && traits::is_separator (path_[0]);
#endif
    }

    template <typename C>
    inline basic_path<C>& basic_path<C>::
    complete ()
    {
      if (relative ())
        *this = current () / *this;

      return *this;
    }

#ifndef _WIN32
    template <typename C>
    inline typename basic_path<C>::string_type basic_path<C>::
    posix_string () const
    {
      return string ();
    }
#endif
  }
}
