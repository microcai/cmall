// file      : libcutl/meta/remove-c.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_META_REMOVE_C_HXX
#define LIBCUTL_META_REMOVE_C_HXX

namespace cutl
{
  namespace meta
  {
    template <typename X>
    struct remove_c
    {
      typedef X r;
    };

    template <typename X>
    struct remove_c<X const>
    {
      typedef X r;
    };
  }
}

#endif // LIBCUTL_META_REMOVE_C_HXX
