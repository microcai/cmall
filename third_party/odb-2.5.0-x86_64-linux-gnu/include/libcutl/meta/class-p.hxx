// file      : libcutl/meta/class-p.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_META_CLASS_HXX
#define LIBCUTL_META_CLASS_HXX

#include <libcutl/meta/answer.hxx>

namespace cutl
{
  namespace meta
  {
    // g++ cannot have these inside class_p.
    //
    template <typename Y> no class_p_test (...);
    template <typename Y> yes class_p_test (void (Y::*) ());

    template <typename X>
    struct class_p
    {
      static bool const r = sizeof (class_p_test<X> (0)) == sizeof (yes);
    };
  }
}

#endif // LIBCUTL_META_CLASS_HXX
