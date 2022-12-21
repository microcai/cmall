// file      : libcutl/compiler/cxx-indenter.txx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    template <typename C>
    cxx_indenter<C>::
    cxx_indenter (code_stream<C>& out)
        : out_ (out),
          buffering_ (false),
          position_ (0),
          paren_balance_ (0),
          spaces_ (2),
          construct_ (con_other),
          do_ (keyword(kw_do)),
          lbrace_ (1, '{'),
          rbrace_ (1, '}')
    {
      indentation_.push (0);

      single_line_blocks_.insert (keyword(kw_if));
      single_line_blocks_.insert (keyword(kw_do));
      single_line_blocks_.insert (keyword(kw_for));
      single_line_blocks_.insert (keyword(kw_else));
      single_line_blocks_.insert (keyword(kw_case));
      single_line_blocks_.insert (keyword(kw_while));

      follow_blocks_.insert (keyword(kw_else));
      follow_blocks_.insert (keyword(kw_case));
      follow_blocks_.insert (keyword(kw_catch));
      follow_blocks_.insert (keyword(kw_default));
    }

    template <typename C>
    void cxx_indenter<C>::
    put (C c)
    {
      // First determine what kind of construct we are in.
      //
      construct new_con (construct_);
      construct old_con (construct_);

      switch (c)
      {
      case '\n':
        {
          if (construct_ == con_pp_dir ||
              construct_ == con_cxx_com)
            construct_ = new_con = con_other;

          break;
        }
      case '#':
        {
          if (construct_ == con_other)
            construct_ = new_con = con_pp_dir;

          break;
        }
      case '\"':
        {
          if (construct_ != con_pp_dir &&
              construct_ != con_c_com &&
              construct_ != con_cxx_com &&
              construct_ != con_char_lit)
          {
            // We might be in an escape sequence.
            //
            bool es (!hold_.empty () && hold_.back () == '\\');

            if (es)
            {
              // Scan the hold sequence backwards to figure out if this
              // backslash is part of this escape sequence or a preceding
              // one.
              //
              for (typename hold::reverse_iterator i (hold_.rbegin () + 1),
                     e (hold_.rend ()); i != e && *i == '\\'; ++i)
                es = !es;
            }

            if (!es)
            {
              if (construct_ == con_string_lit)
                new_con = con_other;
              else
                construct_ = new_con = con_string_lit;
            }
          }

          break;
        }
      case '\'':
        {
          if (construct_ != con_pp_dir &&
              construct_ != con_c_com &&
              construct_ != con_cxx_com &&
              construct_ != con_string_lit)
          {
            // We might be in an escape sequence.
            //
            bool es (!hold_.empty () && hold_.back () == '\\');

            if (es)
            {
              // Scan the hold sequence backwards to figure out if this
              // backslash is part of this escape sequence or a preceding
              // one.
              //
              for (typename hold::reverse_iterator i (hold_.rbegin () + 1),
                     e (hold_.rend ()); i != e && *i == '\\'; ++i)
                es = !es;
            }

            if (!es)
            {
              if (construct_ == con_char_lit)
                new_con = con_other;
              else
                construct_ = new_con = con_char_lit;
            }
          }

          break;
        }
      case '/':
        {
          if (construct_ == con_other)
          {
            if (!hold_.empty () && hold_.back () == '/')
              construct_ = new_con = con_cxx_com;
          }

          if (construct_ == con_c_com)
          {
            if (!hold_.empty () && hold_.back () == '*')
              construct_ = new_con = con_other;
          }

          break;
        }
      case '*':
        {
          if (construct_ == con_other)
          {
            if (!hold_.empty () && hold_.back () == '/')
              construct_ = new_con = con_c_com;
          }

          break;
        }
      default:
        {
          break;
        }
      }

      // Special handling of CPP directives.
      //
      if (construct_ == con_pp_dir)
      {
        write (c);
        position_++;
        return;
      }

      //
      //
      tokenize (c, old_con);


      // Indentation in parenthesis. We don't need to make sure
      // we are not in a comments, etc., because we make sure we
      // don't hold anything in those states.
      //
      if (!hold_.empty () && hold_.back () == '(')
      {
        unbuffer (); // We don't need to hold it anymore.

        if (c == '\n')
          indentation_.push (indentation_.top () + spaces_);
        else
          indentation_.push (position_);
      }


      //
      //
      bool defaulting (false);

      switch (c)
      {
      case '\n':
        {
          if (!indent_stack_.empty () && construct_ == con_other)
            indent_stack_.top ().newline_ = true;

          hold_.push_back (c);
          position_ = 0; // Starting a new line.

          break;
        }
      case '{':
        {
          if (construct_ == con_other)
          {
            if (!indent_stack_.empty ())
            {
              // Pop all the blocks until the one that was indented.
              //
              while (indent_stack_.top ().indentation_ == 0)
                indent_stack_.pop ();

              // Pop the indented block and one level of indentation.
              //
              if (indentation_.size () > 1)
                indentation_.pop ();

              indent_stack_.pop ();
            }

            ensure_new_line ();
            output_indentation ();
            write (c);
            ensure_new_line ();

            indentation_.push (indentation_.top () + spaces_);
          }
          else
            defaulting = true;

          break;
        }
      case '}':
        {
          if (construct_ == con_other)
          {
            if (indentation_.size () > 1)
              indentation_.pop ();

            // Reduce multiple newlines to one.
            //
            while (hold_.size () > 1)
            {
              typename hold::reverse_iterator i (hold_.rbegin ());

              if (*i == '\n' && *(i + 1) == '\n')
                hold_.pop_back ();
              else
                break;
            }

            ensure_new_line ();
            output_indentation ();

            hold_.push_back (c);

            // Add double newline after '}'.
            //
            hold_.push_back ('\n');
            hold_.push_back ('\n');
            position_ = 0;

            if (!indent_stack_.empty ())
            {
              // Pop all the blocks until the one that was indented.
              //
              while (indent_stack_.top ().indentation_ == 0)
                indent_stack_.pop ();

              // Now pop all the indented blocks while also popping the
              // indentation stack. Do it only if the indentation match.
              // If it doesn't then that means this inden_stack entry is
              // for some other, outer block.
              //
              while (!indent_stack_.empty () &&
                     indent_stack_.top ().indentation_ ==
                     indentation_.size ())
              {
                if (indentation_.size () > 1)
                  indentation_.pop ();

                indent_stack_.pop ();
              }
            }

            buffering_ = true;
          }
          else
            defaulting = true;

          break;
        }
      case ';':
        {
          if (construct_ == con_other)
          {
            // for (;;)
            //
            if (!indent_stack_.empty () && paren_balance_ == 0)
            {
              // Pop all the blocks until the one that was indented.
              //
              while (indent_stack_.top ().indentation_ == 0)
                indent_stack_.pop ();

              // Now pop all the indented blocks while also popping the
              // indentation stack. Do it only if the indentation match.
              // If they don't then it means we are inside a block and
              // the stack should be popped after seeing '}', not ';'.
              //
              while (!indent_stack_.empty () &&
                     indent_stack_.top ().indentation_ ==
                     indentation_.size ())
              {
                if (indentation_.size () > 1)
                  indentation_.pop ();

                indent_stack_.pop ();
              }
            }

            if (paren_balance_ != 0)
            {
              // We are inside for (;;) statement. Nothing to do here.
              //
              defaulting = true;
            }
            else
            {
              // Handling '};' case.
              //

              bool brace (false);

              if (hold_.size () > 1 && hold_.back () == '\n')
              {
                bool pop_nl (false);

                for (typename hold::reverse_iterator
                       i (hold_.rbegin ()), e (hold_.rend ());
                     i != e; ++i)
                {
                  if (*i != '\n')
                  {
                    if (*i == '}')
                      brace = pop_nl = true;

                    break;
                  }
                }

                if (pop_nl)
                  while (hold_.back () == '\n')
                    hold_.pop_back ();
              }

              output_indentation ();
              write (c);
              position_++;

              if (brace)
              {
                hold_.push_back ('\n');
                hold_.push_back ('\n');
              }

              ensure_new_line ();
            }
          }
          else
            defaulting = true;

          break;
        }
      case ',':
        {
          if (construct_ == con_other)
          {
            // Handling '},' case.
            //

            bool brace (false);

            if (hold_.size () > 1 && hold_.back () == '\n')
            {
              bool pop_nl (false);

              for (typename hold::reverse_iterator
                     i (hold_.rbegin ()), e (hold_.rend ());
                   i != e; ++i)
              {
                if (*i != '\n')
                {
                  if (*i == '}')
                    brace = pop_nl = true;

                  break;
                }
              }

              if (pop_nl)
                while (hold_.back () == '\n')
                  hold_.pop_back ();
            }

            output_indentation ();
            write (c);
            position_++;

            if (brace)
              hold_.push_back ('\n');
          }
          else
            defaulting = true;

          break;
        }
      case ' ':
        {
          if (construct_ == con_other)
          {
            // Handling '} foo_;' case.
            //
            if (hold_.size () > 1 && hold_.back () == '\n')
            {
              bool pop_nl (false);

              for (typename hold::reverse_iterator
                     i (hold_.rbegin ()), e (hold_.rend ());
                   i != e; ++i)
              {
                if (*i != '\n')
                {
                  if (*i == '}')
                    pop_nl = true;

                  break;
                }
              }

              if (pop_nl)
                while (hold_.back () == '\n')
                  hold_.pop_back ();
            }
          }

          defaulting = true;
          break;
        }
      case '\\':
        {
          if (construct_ != con_pp_dir &&
              construct_ != con_c_com &&
              construct_ != con_cxx_com)
          {
            output_indentation ();
            hold_.push_back (c);
            position_++;
          }
          else
            defaulting = true;

          break;

        }
      case '(':
        {
          if (construct_ == con_other)
          {
            // Hold it so that we can see what's coming next.
            //
            output_indentation ();
            hold_.push_back (c);
            position_++;
            paren_balance_++;
          }
          else
            defaulting = true;
          break;
        }
      case ')':
        {
          if (construct_ == con_other)
          {
            if (indentation_.size () > 1)
              indentation_.pop ();

            if (paren_balance_ > 0)
              paren_balance_--;
          }

          defaulting = true;
          break;
        }
      case '/':
        {
          if (construct_ == con_other)
          {
            output_indentation ();
            hold_.push_back (c);
            position_++;
          }
          else
            defaulting = true;

          break;
        }
      case '*':
        {
          if (construct_ == con_c_com)
          {
            output_indentation ();
            hold_.push_back (c);
            position_++;
          }
          else
            defaulting = true;

          break;
        }
      default:
        {
          defaulting = true;
          break;
        }
      }

      if (defaulting)
      {
        output_indentation ();
        write (c);
        position_++;
      }

      construct_ = new_con;
    }

    template <typename C>
    void cxx_indenter<C>::
    unbuffer ()
    {
      for (; !hold_.empty (); hold_.pop_front ())
        out_.put (hold_.front ());
    }

    template <typename C>
    void cxx_indenter<C>::
    next_token (string const& old, C c)
    {
      // Handle one line indentation blocks (if, else, etc).
      //
      if (single_line_blocks_.find (token_) != single_line_blocks_.end ())
      {
        // Only indent sub-blocks if we are on a new line.
        //
        bool indent (indent_stack_.empty () ||
                     indent_stack_.top ().newline_);

        if (indent)
          indentation_.push (indentation_.top () + spaces_);

        indent_stack_.push (
          indent_block (c == '\n', (indent ? indentation_.size () : 0)));
      }

      // Keep track of the do ... while construct in order to suppress
      // the newline after } and before while.
      //
      if (old == do_ && token_ == lbrace_)
        do_while_state_.push (0);

      if (!do_while_state_.empty ())
      {
        if (token_ == lbrace_)
          do_while_state_.top ()++;

        if (token_ == rbrace_)
          do_while_state_.top ()--;
      }

      // Suppress double newline in the "}else", etc., cases.
      //
      if (old == rbrace_)
      {
        bool dw (!do_while_state_.empty () && do_while_state_.top () == 0);

        if (follow_blocks_.find (token_) != follow_blocks_.end () || dw)
        {
          if (dw)
            do_while_state_.pop ();

          // Reduce double newline after "}" into a single one.
          //
          typename hold::iterator i (hold_.end ()), b (hold_.begin ());

          for (--i; i != b; --i)
          {
            // See if this is the end of the "}\n\n" sequence.
            //
            if (*i == '\n')
            {
              --i;
              if (i != b && *i == '\n')
              {
                --i;
                if (*i == '}')
                {
                  ++i;
                  hold_.erase (i);
                  break;
                }
              }
            }
          }
        }

        // Stop buffering unless we have another closing brace.
        //
        if (token_ != rbrace_)
          buffering_ = false;
      }
    }

    template <typename C>
    void cxx_indenter<C>::
    ensure_new_line ()
    {
      if (hold_.empty () || hold_.back () != '\n')
      {
        hold_.push_back ('\n');
        position_ = 0; // Starting a new line.
      }
    }


    template <typename C>
    void cxx_indenter<C>::
    output_indentation ()
    {
      if (!hold_.empty () && hold_.back () == '\n')
      {
        for (std::size_t i (0); i < indentation_.top (); ++i)
          write (' ');

        position_ += indentation_.top ();
      }
    }

    template <typename C>
    void cxx_indenter<C>::
    write (C c)
    {
      hold_.push_back (c);

      if (!buffering_)
      {
        for (; !hold_.empty (); hold_.pop_front ())
          out_.put (hold_.front ());
      }
    }

    template <typename C>
    void cxx_indenter<C>::
    tokenize (C c, construct old)
    {
      //
      //
      switch (construct_)
      {
      case con_pp_dir:
        {
          if (old == con_other) // Start PP directive
            retire (c);

          return;
        }
      case con_c_com:
        {
          if (old == con_other) // Start C comment.
            lexeme_.clear ();

          return;
        }
      case con_cxx_com:
        {
          if (old == con_other) // Start C++ comment.
            lexeme_.clear ();

          return;
        }
      case con_string_lit:
        {
          if (old == con_other) // Start string literal
            retire (c);

          lexeme_ += c;
          return;
        }
      case con_char_lit:
        {
          if (old == con_other) // Start char literal
            retire (c);

          lexeme_ += c;
          return;
        }
      default:
        break;
      }

      // construct_ == other
      //
      switch (old)
      {
      case con_pp_dir:
        {
          // End  PP directive (newline).
          //
          return;
        }
      case con_c_com:
        {
          // End  C comment.
          //
          return;
        }
      case con_cxx_com:
        {
          // End  C++ comment (newline).
          //
          return;
        }
      case con_string_lit:
        {
          // End string literal (").
          //
          lexeme_ += c;
          return;
        }
      case con_char_lit:
        {
          // End char literal (').
          //
          lexeme_ += c;
          return;
        }
      default:
        break;
      }


      // construct_ == old == other
      //

      switch (char_class (c))
      {
      case cc_alpha:
        {
          if (lexeme_.empty () ||
              char_class (lexeme_[0]) == cc_alpha)
            lexeme_ += c;
          else
          {
            retire (c);
            lexeme_ += c;
          }
          break;
        }
      case cc_digit:
        {
          if (lexeme_.empty ())
            lexeme_ += c;
          else
          {
            char_class_type cc (char_class (lexeme_[0]));

            if (cc == cc_alpha || cc == cc_digit)
              lexeme_ += c;
            else
            {
              retire (c);
              lexeme_ += c;
            }
          }
          break;
        }
      case cc_op_punc:
        {
          retire (c);
          lexeme_ += c;
          break;
        }
      case cc_space:
        {
          retire (c);
          break;
        }
      }
    }

    template <typename C>
    void cxx_indenter<C>::
    retire (C c)
    {
      if (!lexeme_.empty ())
      {
        token_.swap (lexeme_);
        next_token (lexeme_, c);
        lexeme_.clear ();
      }
    }
  }
}
