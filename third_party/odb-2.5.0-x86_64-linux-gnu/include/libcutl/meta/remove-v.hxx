// file      : libcutl/meta/remove-v.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_META_REMOVE_V_HXX
#define LIBCUTL_META_REMOVE_V_HXX

namespace cutl
{
  namespace meta
  {
    template <typename X>
    struct remove_v
    {
      typedef X r;
    };

    template <typename X>
    struct remove_v<X volatile>
    {
      typedef X r;
    };
  }
}

#endif // LIBCUTL_META_REMOVE_V_HXX
