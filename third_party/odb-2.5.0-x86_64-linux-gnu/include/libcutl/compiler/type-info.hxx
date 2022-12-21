// file      : libcutl/compiler/type-info.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_COMPILER_TYPE_INFO_HXX
#define LIBCUTL_COMPILER_TYPE_INFO_HXX

#include <map>
#include <vector>
#include <typeinfo> // std::type_info

#include <libcutl/exception.hxx>
#include <libcutl/static-ptr.hxx>
#include <libcutl/compiler/type-id.hxx>

#include <libcutl/export.hxx>

namespace cutl
{
  namespace compiler
  {
    //
    //
    class type_info;
    typedef type_info type_info_t;


    //
    //
    class base_info
    {
    public:
      base_info (type_id const&);

    public:
      type_info_t const&
      type_info () const;

    private:
      type_id type_id_;
      mutable type_info_t const* type_info_;
    };

    typedef base_info base_info_t;


    //
    //
    class type_info
    {
      typedef std::vector<base_info> bases;

    public:
      typedef
      bases::const_iterator
      base_iterator;

    public:
      type_info (type_id_t const&);

      type_id_t
      type_id () const;

      base_iterator
      begin_base () const;

      base_iterator
      end_base () const;

      void
      add_base (type_id_t const&);

    private:
      type_id_t type_id_;
      bases bases_;
    };


    //
    //
    class no_type_info: exception {};

    LIBCUTL_EXPORT type_info const&
    lookup (type_id const&);

    type_info const&
    lookup (std::type_info const&);

    template <typename X>
    type_info const&
    lookup (X const volatile&);

    template<typename X>
    type_info const&
    lookup ();

    LIBCUTL_EXPORT void
    insert (type_info const&);

    namespace bits
    {
      struct default_type_info_id {};
      typedef std::map<type_id, type_info> type_info_map;
      static static_ptr<type_info_map, default_type_info_id> type_info_map_;
    }
  }
}

#include <libcutl/compiler/type-info.ixx>

#endif // LIBCUTL_COMPILER_TYPE_INFO_HXX
