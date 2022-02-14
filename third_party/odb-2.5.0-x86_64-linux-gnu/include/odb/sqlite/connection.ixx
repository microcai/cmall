// file      : odb/sqlite/connection.ixx
// license   : GNU GPL v2; see accompanying LICENSE file

namespace odb
{
  namespace sqlite
  {
    // active_objects
    //
    inline void active_object::
    list_add ()
    {
      next_ = conn_.active_objects_;
      conn_.active_objects_ = this;

      if (next_ != 0)
        next_->prev_ = this;
    }

    inline void active_object::
    list_remove ()
    {
      (prev_ == 0 ? conn_.active_objects_ : prev_->next_) = next_;

      if (next_ != 0)
        next_->prev_ = prev_;

      prev_ = 0;
      next_ = this;
    }

    // connection
    //
    inline database& connection::
    database ()
    {
      return static_cast<connection_factory&> (factory_).database ();
    }

    template <typename T>
    inline prepared_query<T> connection::
    prepare_query (const char* n, const char* q)
    {
      return prepare_query<T> (n, query<T> (q));
    }

    template <typename T>
    inline prepared_query<T> connection::
    prepare_query (const char* n, const std::string& q)
    {
      return prepare_query<T> (n, query<T> (q));
    }

    template <typename T>
    inline prepared_query<T> connection::
    prepare_query (const char* n, const sqlite::query_base& q)
    {
      return query_<T, id_sqlite>::call (*this, n, q);
    }

    template <typename T>
    inline prepared_query<T> connection::
    prepare_query (const char* n, const odb::query_base& q)
    {
      // Translate to native query.
      //
      return prepare_query<T> (n, sqlite::query_base (q));
    }
  }
}
