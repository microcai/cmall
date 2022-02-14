// file      : cutl/meta/remove-v.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_META_REMOVE_V_HXX
#define CUTL_META_REMOVE_V_HXX

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

#endif // CUTL_META_REMOVE_V_HXX
