// file      : cutl/meta/class-p.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_META_CLASS_HXX
#define CUTL_META_CLASS_HXX

#include <cutl/meta/answer.hxx>

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

#endif // CUTL_META_CLASS_HXX
