// file      : odb/pgsql/statement-cache.txx
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/pgsql/database.hxx>

namespace odb
{
  namespace pgsql
  {
    template <typename T>
    typename object_traits_impl<T, id_pgsql>::statements_type&
    statement_cache::
    find_object ()
    {
      typedef
      typename object_traits_impl<T, id_pgsql>::statements_type
      statements_type;

      // Clear the cache if the database version has changed. This
      // makes sure we don't re-use statements that correspond to
      // the old schema.
      //
      if (version_seq_ != conn_.database ().schema_version_sequence ())
      {
        map_.clear ();
        version_seq_ = conn_.database ().schema_version_sequence ();
      }

      map::iterator i (map_.find (&typeid (T)));

      if (i != map_.end ())
        return static_cast<statements_type&> (*i->second);

      details::shared_ptr<statements_type> p (
        new (details::shared) statements_type (conn_));

      map_.insert (map::value_type (&typeid (T), p));
      return *p;
    }

    template <typename T>
    view_statements<T>& statement_cache::
    find_view ()
    {
      // We don't cache any statements for views so no need to clear
      // the cache.

      map::iterator i (map_.find (&typeid (T)));

      if (i != map_.end ())
        return static_cast<view_statements<T>&> (*i->second);

      details::shared_ptr<view_statements<T> > p (
        new (details::shared) view_statements<T> (conn_));

      map_.insert (map::value_type (&typeid (T), p));
      return *p;
    }
  }
}
