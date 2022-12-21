// file      : libcutl/meta/remove-p.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_META_REMOVE_P_HXX
#define LIBCUTL_META_REMOVE_P_HXX

namespace cutl
{
  namespace meta
  {
    template <typename X>
    struct remove_p
    {
      typedef X r;
    };

    template <typename X>
    struct remove_p<X*>
    {
      typedef X r;
    };
  }
}

#endif // LIBCUTL_META_REMOVE_P_HXX
