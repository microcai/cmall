// file      : odb/pgsql/no-id-object-result.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_PGSQL_NO_ID_OBJECT_RESULT_HXX
#define ODB_PGSQL_NO_ID_OBJECT_RESULT_HXX

#include <odb/pre.hxx>

#include <cstddef> // std::size_t

#include <odb/schema-version.hxx>
#include <odb/no-id-object-result.hxx>

#include <odb/details/shared-ptr.hxx>

#include <odb/pgsql/version.hxx>
#include <odb/pgsql/forward.hxx> // query_base
#include <odb/pgsql/statement.hxx>
#include <odb/pgsql/traits-calls.hxx>

namespace odb
{
  namespace pgsql
  {
    template <typename T>
    class no_id_object_result_impl: public odb::no_id_object_result_impl<T>
    {
    public:
      typedef odb::no_id_object_result_impl<T> base_type;

      typedef typename base_type::object_type object_type;
      typedef typename base_type::pointer_type pointer_type;

      typedef object_traits_impl<object_type, id_pgsql> object_traits;
      typedef typename base_type::pointer_traits pointer_traits;

      typedef typename object_traits::statements_type statements_type;

      virtual
      ~no_id_object_result_impl ();

      no_id_object_result_impl (const query_base&,
                                details::shared_ptr<select_statement>,
                                statements_type&,
                                const schema_version_migration*);

      virtual void
      load (object_type&);

      virtual void
      next ();

      virtual void
      cache ();

      virtual std::size_t
      size ();

      virtual void
      invalidate ();

      using base_type::current;

    private:
      details::shared_ptr<select_statement> statement_;
      statements_type& statements_;
      object_traits_calls<object_type> tc_;
      std::size_t count_;
    };
  }
}

#include <odb/pgsql/no-id-object-result.txx>

#include <odb/post.hxx>

#endif // ODB_PGSQL_NO_ID_OBJECT_RESULT_HXX
