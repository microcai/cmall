// file      : libcutl/re.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_RE_HXX
#define LIBCUTL_RE_HXX

#include <string>
#include <ostream>

#include <libcutl/exception.hxx>

#include <libcutl/export.hxx>

namespace cutl
{
  namespace re
  {
    struct LIBCUTL_EXPORT format_base: exception
    {
      format_base (std::string const& d): description_ (d) {}
      ~format_base () noexcept {}

      std::string const&
      description () const
      {
        return description_;
      }

      virtual char const*
      what () const noexcept;

    protected:
      std::string description_;
    };

    template <typename C>
    struct basic_format: format_base
    {
      basic_format (std::basic_string<C> const& e, std::string const& d)
          : format_base (d), regex_ (e) {}
      ~basic_format () noexcept {}

      std::basic_string<C> const&
      regex () const
      {
        return regex_;
      }

    private:
      std::basic_string<C> regex_;
    };

    typedef basic_format<char> format;
    typedef basic_format<wchar_t> wformat;

    // Regular expression pattern.
    //
    template <typename C>
    struct basic_regex
    {
      typedef std::basic_string<C> string_type;

      ~basic_regex ();

      basic_regex (): impl_ (0) {init (0, false);}

      explicit
      basic_regex (string_type const& s, bool icase = false)
          : impl_ (0)
      {
        init (&s, icase);
      }

      basic_regex&
      operator= (string_type const& s)
      {
        init (&s, false);
        return *this;
      }

      basic_regex&
      assign (string_type const& s, bool icase = false)
      {
        init (&s, icase);
        return *this;
      }

      basic_regex (basic_regex const&);

      basic_regex&
      operator= (basic_regex const&);

    public:
      bool
      match (string_type const&) const;

      bool
      search (string_type const&) const;

      string_type
      replace (string_type const& s,
               string_type const& sub,
               bool first_only = false) const;

    public:
      string_type const&
      str () const
      {
        return str_;
      }

      bool
      empty () const
      {
        return str_.empty ();
      }

    private:
      void
      init (string_type const*, bool);

    private:
      struct impl;

      string_type str_; // Text representation of regex.
      impl* impl_;
    };

    template <typename C>
    inline std::basic_ostream<C>&
    operator<< (std::basic_ostream<C>& os, basic_regex<C> const& r)
    {
      return os << r.str ();
    }

    typedef basic_regex<char> regex;
    typedef basic_regex<wchar_t> wregex;

    // Regular expression pattern and substituation.
    //
    template <typename C>
    struct basic_regexsub
    {
      typedef basic_regex<C> regex_type;
      typedef std::basic_string<C> string_type;

      basic_regexsub () {}

      // Expression is of the form /regex/substitution/ where '/' can
      // be replaced with any delimiter. Delimiters must be escaped in
      // regex and substitution using back slashes (e.g., "\/"). Back
      // slashes themselves can be escaped using the double back slash
      // sequence (e.g., "\\").
      //
      explicit
      basic_regexsub (string_type const& e) {init (e);}

      basic_regexsub (string_type const& regex, string_type const& sub)
          : regex_ (regex), sub_ (sub)
      {
      }

      basic_regexsub (regex_type const& regex, string_type const& sub)
          : regex_ (regex), sub_ (sub)
      {
      }

      basic_regexsub&
      operator= (string_type const& e)
      {
        init (e);
        return *this;
      }

    public:
      bool
      match (string_type const& s) const
      {
        return regex_.match (s);
      }

      bool
      search (string_type const& s) const
      {
        return regex_.search (s);
      }

      string_type
      replace (string_type const& s, bool first_only = false) const
      {
        return regex_.replace (s, sub_, first_only);
      }

    public:
      const regex_type&
      regex () const
      {
        return regex_;
      }

      const string_type&
      substitution () const
      {
        return sub_;
      }

      bool
      empty () const
      {
        return sub_.empty () && regex_.empty ();
      }

    private:
      void
      init (string_type const&);

    private:
      regex_type regex_;
      string_type sub_;
    };

    typedef basic_regexsub<char> regexsub;
    typedef basic_regexsub<wchar_t> wregexsub;

    // Once-off regex execution.
    //
    template <typename C>
    inline bool
    match (std::basic_string<C> const& s, std::basic_string<C> const& regex)
    {
      basic_regex<C> r (regex);
      return r.match (s);
    }

    template <typename C>
    inline bool
    search (std::basic_string<C> const& s, std::basic_string<C> const& regex)
    {
      basic_regex<C> r (regex);
      return r.search (s);
    }

    template <typename C>
    inline std::basic_string<C>
    replace (std::basic_string<C> const& s,
             std::basic_string<C> const& regex,
             std::basic_string<C> const& sub,
             bool first_only = false)
    {
      basic_regex<C> r (regex);
      return r.replace (s, sub, first_only);
    }

    template <typename C>
    inline std::basic_string<C>
    replace (std::basic_string<C> const& s,
             std::basic_string<C> const& regexsub, // /regex/subst/
             bool first_only = false)
    {
      basic_regexsub<C> r (regexsub);
      return r.replace (s, first_only);
    }

    // Utility function for parsing expressions in the form /regex/subst/
    // where '/' can be replaced with any delimiter. This function handles
    // escaping. It return the position of the next delimiter and stores
    // the unescaped chunk in result or throws the format exception if
    // the expression is invalid.
    //
    template <typename C>
    typename std::basic_string<C>::size_type
    parse (std::basic_string<C> const& s,
           typename std::basic_string<C>::size_type start,
           std::basic_string<C>& result);
  }
}

#include <libcutl/re/re.txx>

#endif // LIBCUTL_RE_HXX
