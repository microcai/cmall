// file      : libcutl/meta/remove-cv.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_META_REMOVE_CV_HXX
#define LIBCUTL_META_REMOVE_CV_HXX

#include <libcutl/meta/remove-c.hxx>
#include <libcutl/meta/remove-v.hxx>

namespace cutl
{
  namespace meta
  {
    template <typename X>
    struct remove_cv
    {
      typedef typename remove_v<typename remove_c<X>::r>::r r;
    };
  }
}

#endif // LIBCUTL_META_REMOVE_CV_HXX
