// file      : odb/sqlite/connection.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_CONNECTION_HXX
#define ODB_SQLITE_CONNECTION_HXX

#include <odb/pre.hxx>

#include <sqlite3.h>

#include <odb/connection.hxx>

#include <odb/details/mutex.hxx>
#include <odb/details/condition.hxx>
#include <odb/details/shared-ptr.hxx>
#include <odb/details/unique-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/forward.hxx>
#include <odb/sqlite/query.hxx>
#include <odb/sqlite/tracer.hxx>
#include <odb/sqlite/transaction-impl.hxx>
#include <odb/sqlite/auto-handle.hxx>

#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    class statement_cache;
    class connection_factory;

    class connection;
    typedef details::shared_ptr<connection> connection_ptr;

    // SQLite "active object", i.e., an object that needs to be
    // "cleared" before the transaction can be committed and the
    // connection release. These form a doubly-linked list.
    //
    class LIBODB_SQLITE_EXPORT active_object
    {
    public:
      // This function should remove the object from the list, since
      // it shall no longer be "active".
      //
      virtual void
      clear () = 0;

    protected:
      active_object (connection& c): prev_ (0), next_ (this), conn_ (c) {}

      void
      list_add ();

      void
      list_remove ();

    protected:
      // prev_ == 0 means we are the first element.
      // next_ == 0 means we are the last element.
      // next_ == this means we are not on the list (prev_ should be 0).
      //
      active_object* prev_;
      active_object* next_;

      connection& conn_;
    };

    class LIBODB_SQLITE_EXPORT connection: public odb::connection
    {
    public:
      typedef sqlite::statement_cache statement_cache_type;
      typedef sqlite::database database_type;

      virtual
      ~connection ();

      connection (connection_factory&, int extra_flags = 0);
      connection (connection_factory&, sqlite3* handle);

      database_type&
      database ();

    public:
      virtual transaction_impl*
      begin ();

      transaction_impl*
      begin_immediate ();

      transaction_impl*
      begin_exclusive ();

    public:
      using odb::connection::execute;

      virtual unsigned long long
      execute (const char* statement, std::size_t length);

      // Query preparation.
      //
    public:
      template <typename T>
      prepared_query<T>
      prepare_query (const char* name, const char*);

      template <typename T>
      prepared_query<T>
      prepare_query (const char* name, const std::string&);

      template <typename T>
      prepared_query<T>
      prepare_query (const char* name, const sqlite::query_base&);

      template <typename T>
      prepared_query<T>
      prepare_query (const char* name, const odb::query_base&);

      // SQL statement tracing.
      //
    public:
      typedef sqlite::tracer tracer_type;

      void
      tracer (tracer_type& t)
      {
        odb::connection::tracer (t);
      }

      void
      tracer (tracer_type* t)
      {
        odb::connection::tracer (t);
      }

      using odb::connection::tracer;

    public:
      sqlite3*
      handle ()
      {
        return handle_;
      }

      statement_cache_type&
      statement_cache ()
      {
        return *statement_cache_;
      }

      // Wait for the locks to be released via unlock notification. Can
      // be called after getting SQLITE_LOCKED_SHAREDCACHE.
      //
      void
      wait ();

    public:
      // Reset active statements.
      //
      void
      clear ();

    private:
      connection (const connection&);
      connection& operator= (const connection&);

    private:
      void
      init ();

    private:
      auto_handle<sqlite3> handle_;

      // Keep statement_cache_ after handle_ so that it is destroyed before
      // the connection is closed.
      //
      details::unique_ptr<statement_cache_type> statement_cache_;

      // Unlock notification machinery.
      //
    private:
      bool unlocked_;
      details::mutex unlock_mutex_;
      details::condition unlock_cond_;

      friend void
      connection_unlock_callback (void**, int);

    private:
      friend class transaction_impl; // invalidate_results()

      // Linked list of active objects currently associated
      // with this connection.
      //
    private:
      friend class active_object;
      active_object* active_objects_;
    };

    class LIBODB_SQLITE_EXPORT connection_factory:
      public odb::connection_factory
    {
    public:
      typedef sqlite::database database_type;

      virtual void
      database (database_type&);

      database_type&
      database () {return *db_;}

      virtual connection_ptr
      connect () = 0;

      virtual
      ~connection_factory ();

      connection_factory (): db_ (0) {}

      // Needed to break the circular connection_factory-database dependency
      // (odb::connection_factory has the odb::database member).
      //
    protected:
      database_type* db_;
    };
  }
}

#include <odb/sqlite/connection.ixx>

#include <odb/post.hxx>

#endif // ODB_SQLITE_CONNECTION_HXX
