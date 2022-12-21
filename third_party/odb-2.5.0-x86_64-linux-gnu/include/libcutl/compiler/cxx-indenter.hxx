// file      : libcutl/compiler/cxx-indenter.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_COMPILER_CXX_INDENTER_HXX
#define LIBCUTL_COMPILER_CXX_INDENTER_HXX

#include <set>
#include <stack>
#include <deque>
#include <string>
#include <cstddef> // std::size_t

#include <libcutl/compiler/code-stream.hxx>

namespace cutl
{
  namespace compiler
  {
    template <typename C>
    class cxx_indenter: public code_stream<C>
    {
    public:
      cxx_indenter (code_stream<C>& out);

    private:
      cxx_indenter (cxx_indenter const&);

      cxx_indenter&
      operator= (cxx_indenter const&);

    public:
      virtual void
      put (C);

      virtual void
      unbuffer ();

    private:
      typedef std::basic_string<C> string;

      enum construct
      {
        con_other,
        con_pp_dir,
        con_c_com,
        con_cxx_com,
        con_string_lit,
        con_char_lit
      };

    private:
      void
      next_token (string const& old, C);

      void
      ensure_new_line ();

      void
      output_indentation ();

      void
      write (C);

    private:
      void
      tokenize (C, construct old);

      void
      retire (C);

    private:
      enum char_class_type
      {
        cc_alpha,   // Alpha + '_'.
        cc_digit,
        cc_op_punc, // Operator or punctuation.
        cc_space
      };

      static char_class_type
      char_class (C);

    private:
      enum keyword_type
      {
        kw_if,
        kw_do,
        kw_for,
        kw_else,
        kw_case,
        kw_while,
        kw_catch,
        kw_default
      };

      static C const*
      keyword (keyword_type);

    private:
      code_stream<C>& out_;
      bool buffering_;            // True if write() should buffer the char.
      std::size_t position_;      // Current position on the line.
      std::size_t paren_balance_; // ( ) balance.
      std::stack<std::size_t> indentation_;
      std::size_t spaces_;
      bool suppress_nl_;
      construct construct_;

      // Special state stack for the do-while construct. The presence
      // of an element in the stack indicates that we are in a braced
      // do-while construct. The value of the element is the brace
      // balance.
      std::stack<std::size_t> do_while_state_;

      typedef std::deque<C> hold;
      hold hold_;

    private:
      string token_;  // previously fully recognized token
      string lexeme_; // current lexeme (accumulator)

      // Keywords that may be folowed by a single-line block, e.g., if,
      // else, etc.
      //
      std::set<string> single_line_blocks_;

      // Keywords that may follow (and be related) to a previous block,
      // e.g., else, case, catch.
      //
      std::set<string> follow_blocks_;

      string do_;
      string lbrace_;
      string rbrace_;

    private:
      // Single-line indented blocks such as if, else, while, etc. The
      // newline flag indicates whether a new line has been seen after
      // the keyword. This is needed to properly distinguish cases such
      // as:
      //
      // else if (...)
      //   foo ();
      //
      // else
      //   if (...)
      //     foo ();
      //
      struct indent_block
      {
        indent_block (bool newline, std::size_t indentation)
            : newline_ (newline), indentation_ (indentation)
        {
        }

        bool newline_;
        std::size_t indentation_; // Size of the indentation_ stack
                                  // corresponding to this block, or
                                  // 0 if it is not indented.
      };

      std::stack<indent_block> indent_stack_;
    };
  }
}

#include <libcutl/compiler/cxx-indenter.ixx>
#include <libcutl/compiler/cxx-indenter.txx>

#endif // LIBCUTL_COMPILER_CXX_INDENTER_HXX
