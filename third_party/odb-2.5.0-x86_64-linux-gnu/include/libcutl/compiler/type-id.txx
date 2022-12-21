// file      : libcutl/compiler/type-id.txx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    template <typename X>
    inline
    type_id::
    type_id (X const volatile& x)
        : ti_ (&typeid (x))
    {
    }
  }
}
