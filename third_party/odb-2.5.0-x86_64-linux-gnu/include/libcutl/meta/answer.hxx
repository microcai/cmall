// file      : libcutl/meta/answer.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_META_ANSWER_HXX
#define LIBCUTL_META_ANSWER_HXX

namespace cutl
{
  namespace meta
  {
    struct yes
    {
      char filling;
    };

    struct no
    {
      char filling[2];
    };
  }
}

#endif // LIBCUTL_META_ANSWER_HXX
