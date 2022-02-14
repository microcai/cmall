// file      : cutl/meta/polymorphic-p.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_META_POLYMORPHIC_HXX
#define CUTL_META_POLYMORPHIC_HXX

#include <cutl/details/config.hxx>

#include <cutl/meta/class-p.hxx>
#include <cutl/meta/remove-cv.hxx>

namespace cutl
{
  namespace meta
  {
    template <typename CVX>
    struct polymorphic_p
    {
      typedef typename remove_cv<CVX>::r X;

      template <typename Y, bool C>
      struct impl
      {
        static const bool r = false;
      };

      template <typename Y>
      struct impl<Y, true>
      {
        struct t1: Y
        {
          t1 ();
        };

        struct t2: Y
        {
          t2 ();

          virtual
          ~t2 () LIBCUTL_NOTHROW_NOEXCEPT;
        };

        static const bool r = sizeof (t1) == sizeof (t2);
      };

      static const bool r = impl<X, class_p<X>::r>::r;
    };
  }
}

#endif // CUTL_META_POLYMORPHIC_HXX
