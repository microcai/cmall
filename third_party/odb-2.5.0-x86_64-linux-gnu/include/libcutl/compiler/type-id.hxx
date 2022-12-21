// file      : libcutl/compiler/type-id.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_COMPILER_TYPE_ID_HXX
#define LIBCUTL_COMPILER_TYPE_ID_HXX

#include <typeinfo> // std::type_info

namespace cutl
{
  namespace compiler
  {
    class type_id
    {
    public:
      template<typename X>
      type_id (X const volatile&);

      type_id (std::type_info const&);

    public:
      char const*
      name () const;

      friend bool
      operator== (type_id const&, type_id const&);

      friend bool
      operator!= (type_id const&, type_id const&);

      friend bool
      operator< (type_id const&, type_id const&);

    private:
      std::type_info const* ti_;
    };

    typedef type_id type_id_t;
  }
}

#include <libcutl/compiler/type-id.ixx>
#include <libcutl/compiler/type-id.txx>

#endif // LIBCUTL_COMPILER_TYPE_ID_HXX
