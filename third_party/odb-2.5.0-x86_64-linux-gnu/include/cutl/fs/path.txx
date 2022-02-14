// file      : cutl/fs/path.txx
// license   : MIT; see accompanying LICENSE file

#include <vector>

namespace cutl
{
  namespace fs
  {
    template <typename C>
    basic_path<C> basic_path<C>::
    leaf () const
    {
      size_type p (traits::rfind_separator (path_));

      return p != string_type::npos
        ? basic_path (path_.c_str () + p + 1, path_.size () - p - 1)
        : *this;
    }

    template <typename C>
    basic_path<C> basic_path<C>::
    directory () const
    {
      if (root ())
        return basic_path ();

      size_type p (traits::rfind_separator (path_));

      // Include the trailing slash so that we get correct behavior
      // if directory is root.
      //
      return p != string_type::npos
        ? basic_path (path_.c_str (), p + 1)
        : basic_path ();
    }

    template <typename C>
    basic_path<C> basic_path<C>::
    base () const
    {
      size_type i (path_.size ());

      for (; i > 0; --i)
      {
        if (path_[i - 1] == '.')
          break;

        if (traits::is_separator (path_[i - 1]))
        {
          i = 0;
          break;
        }
      }

      // Weed out paths like ".txt" and "/.txt"
      //
      if (i > 1 && !traits::is_separator (path_[i - 2]))
      {
        return basic_path (path_.c_str (), i - 1);
      }
      else
        return *this;
    }

#ifdef _WIN32
    template <typename C>
    typename basic_path<C>::string_type basic_path<C>::
    posix_string () const
    {
      if (absolute ())
        throw invalid_basic_path<C> (path_);

      string_type r (path_);

      // Translate Windows-style separators to the POSIX ones.
      //
      for (size_type i (0), n (r.size ()); i != n; ++i)
        if (r[i] == '\\')
          r[i] = '/';

      return r;
    }
#endif

    template <typename C>
    basic_path<C>& basic_path<C>::
    operator/= (basic_path<C> const& r)
    {
      if (r.absolute ())
        throw invalid_basic_path<C> (r.path_);

      if (path_.empty () || r.path_.empty ())
      {
        path_ += r.path_;
        return *this;
      }

      if (!traits::is_separator (path_[path_.size () - 1]))
        path_ += traits::directory_separator;

      path_ += r.path_;

      return *this;
    }

    template <typename C>
    basic_path<C>& basic_path<C>::
    normalize ()
    {
      if (empty ())
        return *this;

      bool abs (absolute ());

      typedef std::vector<string_type> paths;
      paths ps;

      for (size_type b (0), e (traits::find_separator (path_)),
             n (path_.size ());;
           e = traits::find_separator (path_, b))
      {
        string_type s (path_, b, e == string_type::npos ? e : e - b);
        ps.push_back (s);

        if (e == string_type::npos)
          break;

        ++e;

        while (e < n && traits::is_separator (path_[e]))
          ++e;

        if (e == n)
          break;

        b = e;
      }

      // First collapse '.' and '..'.
      //
      paths r;

      for (typename paths::const_iterator i (ps.begin ()), e (ps.end ());
           i != e; ++i)
      {
        string_type const& s (*i);
        size_type n (s.size ());

        if (n == 1 && s[0] == '.')
          continue;

        if (n == 2 && s[0] == '.' && s[1] == '.')
        {
          // Pop the last directory from r unless it is '..'.
          //
          if (!r.empty ())
          {
            string_type const& s1 (r.back ());

            if (!(s1.size () == 2 && s1[0] == '.' && s1[1] == '.'))
            {
              // Cannot go past the root directory.
              //
              if (abs && r.size () == 1)
                throw invalid_basic_path<C> (path_);

              r.pop_back ();
              continue;
            }
          }
        }

        r.push_back (s);
      }

      // Reassemble the path.
      //
      string_type p;

      for (typename paths::const_iterator i (r.begin ()), e (r.end ());
           i != e;)
      {
        p += *i;

        if (++i != e)
          p += traits::directory_separator;
      }

      if (p.empty () && !r.empty ())
        p += traits::directory_separator; // Root directory.

      path_.swap (p);
      return *this;
    }

    template <typename C>
    void basic_path<C>::
    init ()
    {
      // Strip trailing slashes except for the case where the single
      // slash represents the root directory.
      //
      size_type n (path_.size ());
      for (; n > 1 && traits::is_separator (path_[n - 1]); --n) ;
      path_.resize (n);
    }
  }
}
