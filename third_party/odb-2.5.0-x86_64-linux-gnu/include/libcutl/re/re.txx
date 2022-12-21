// file      : libcutl/re/re.txx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace re
  {
    //
    // basic_regexsub
    //
    template <typename C>
    void basic_regexsub<C>::
    init (string_type const& s)
    {
      string_type r;
      typename string_type::size_type p (parse (s, 0, r));
      regex_ = r;
      p = parse (s, p, sub_);
      if (p + 1 < s.size ())
        throw basic_format<C> (s, "junk after third delimiter");
    }

    //
    // parse()
    //
    template <typename C>
    typename std::basic_string<C>::size_type
    parse (std::basic_string<C> const& s,
           typename std::basic_string<C>::size_type p,
           std::basic_string<C>& r)
    {
      r.clear ();
      typename std::basic_string<C>::size_type n (s.size ());

      if (p >= n)
        throw basic_format<C> (s, "empty expression");

      C d (s[p++]);

      for (; p < n; ++p)
      {
        if (s[p] == d)
          break;

        if (s[p] == '\\')
        {
          if (++p < n)
          {
            // Pass the escape sequence through unless it is the delimiter.
            //
            if (s[p] != d)
              r += '\\';

            r += s[p];
          }
          // else {We ran out of stuff before finding the delimiter.}
        }
        else
          r += s[p];
      }

      if (p == n)
        throw basic_format<C> (s, "missing closing delimiter");

      return p;
    }
  }
}
