// file      : libcutl/compiler/sloc-counter.txx
// license   : MIT; see accompanying LICENSE file

#include <cctype>  // std::isspace

namespace cutl
{
  namespace compiler
  {
    template <typename C>
    sloc_counter<C>::
    sloc_counter (code_stream<C>& out)
        : out_ (out),
          count_ (0),
          prev_ ('\0'),
          code_counted_ (false),
          construct_ (con_code)
    {
    }

    template <typename C>
    void sloc_counter<C>::
    put (C c)
    {
      construct old (construct_);

      switch (construct_)
      {
      case con_code:
        {
          code (c);
          break;
        }
      case con_c_com:
        {
          c_comment (c);
          break;
        }
      case con_cxx_com:
        {
          cxx_comment (c);
          break;
        }
      case con_char_lit:
        {
          char_literal (c);
          break;
        }
      case con_string_lit:
        {
          string_literal (c);
          break;
        }
      }

      // There are cases when a previous character has been already
      // 'used' and therefore can not be used again. Good example
      // would be '/* *//'. Here, the second slash doesn't start
      // C++ comment since it was already used by C comment.
      //
      // To account for this we are going to set prev_ to '\0' when
      // the mode changes.
      //

      prev_ = (old == construct_) ? c : '\0';

      out_.put (c);
    }

    template <typename C>
    void sloc_counter<C>::
    unbuffer ()
    {
    }

    template <typename C>
    void sloc_counter<C>::
    code (C c)
    {
      bool count (true);

      switch (c)
      {
      case '/':
        {
          if (prev_ == '/')
          {
            construct_ = con_cxx_com;
            count = false;
          }
          else
          {
            // This slash can be a beginning of a comment but we
            // yet have no way to know. Will have to examine it later
            // (see below).
            //
            count = false;
          }

          break;
        }
      case '*':
        {
          if (prev_ == '/')
          {
            construct_ = con_c_com;
            count = false;
          }
          break;
        }
      case '\'':
        {
          construct_ = con_char_lit;
          break;
        }
      case '"':
        {
          construct_ = con_string_lit;
          break;
        }
      case '\n':
        {
          code_counted_ = false; // Reset for a new line.
          count = false;
          break;
        }
      default:
        {
          if (std::isspace (c))
            count = false;
          break;
        }
      }

      if (!code_counted_)
      {
        if (count)
        {
          count_++;
          code_counted_ = true;
        }
        else if (prev_ == '/' && construct_ == con_code)
        {
          // This condition accounts for the fact that we cannot count
          // '/' right away since it can be a beginning of a comment.
          //
          count_++;
          code_counted_ = (c != '\n');
        }
      }
    }

    template <typename C>
    void sloc_counter<C>::
    c_comment (C c)
    {
      switch (c)
      {
      case '/':
        {
          if (prev_ == '*')
            construct_ = con_code;
          break;
        }
      case '\n':
        {
          code_counted_ = false; // Reset for a new line.
          break;
        }
      }
    }

    template <typename C>
    void sloc_counter<C>::
    cxx_comment (C c)
    {
      switch (c)
      {
      case '\n':
        {
          construct_ = con_code;
          code_counted_ = false; // Reset for a new line.
          break;
        }
      }
    }

    template <typename C>
    void sloc_counter<C>::
    char_literal (C c)
    {
      switch (c)
      {
      case '\'':
        {
          if (prev_ != '\\')
            construct_ = con_code;
          break;
        }
      }
    }

    template <typename C>
    void sloc_counter<C>::
    string_literal (C c)
    {
      switch (c)
      {
      case '"':
        {
          if (prev_ != '\\')
            construct_ = con_code;
          break;
        }
      case '\n':
        {
          count_++;
          break;
        }
      }
    }
  }
}
