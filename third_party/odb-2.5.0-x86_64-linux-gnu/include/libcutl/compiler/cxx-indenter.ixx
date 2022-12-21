// file      : libcutl/compiler/cxx-indenter.ixx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    template <typename C>
    inline typename cxx_indenter<C>::char_class_type cxx_indenter<C>::
    char_class (C c)
    {
      switch (c)
      {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        return cc_digit;

      case '!':
      case '%':
      case '^':
      case '&':
      case '*':
      case '(':
      case ')':
      case '-':
      case '+':
      case '=':
      case '{':
      case '}':
      case '|':
      case '~':
      case '[':
      case ']':
      case '\\':
      case ';':
      case '\'':
      case ':':
      case '"':
      case '<':
      case '>':
      case '?':
      case ',':
      case '.':
      case '/':
        return cc_op_punc;

      case ' ':
      case '\n':
      case '\t':
      case '\f':
      case '\r':
      case '\v':
        return cc_space;

      default:
        return cc_alpha;
      }
    }
  }
}
