// file      : odb/pgsql/simple-object-statements.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_PGSQL_SIMPLE_OBJECT_STATEMENTS_HXX
#define ODB_PGSQL_SIMPLE_OBJECT_STATEMENTS_HXX

#include <odb/pre.hxx>

#include <vector>
#include <cassert>
#include <cstddef> // std::size_t

#include <odb/forward.hxx>
#include <odb/traits.hxx>

#include <odb/details/shared-ptr.hxx>

#include <odb/pgsql/version.hxx>
#include <odb/pgsql/forward.hxx>
#include <odb/pgsql/pgsql-types.hxx>
#include <odb/pgsql/binding.hxx>
#include <odb/pgsql/statement.hxx>
#include <odb/pgsql/statements-base.hxx>

#include <odb/pgsql/details/export.hxx>

namespace odb
{
  namespace pgsql
  {
    // The extra_statement_cache class is only defined (and used) in
    // the generated source file. However, object_statements may be
    // referenced from another source file in the case of a polymorphic
    // hierarchy (though in this case the extra statement cache is
    // not used). As a result, we cannot have a by-value member and
    // instead will store a pointer and lazily allocate the cache if
    // and when needed. We will also need to store a pointer to the
    // deleter function which will be initialized during allocation
    // (at that point we know that the cache class is defined).
    //
    template <typename T, typename I, typename ID>
    struct extra_statement_cache_ptr
    {
      typedef I image_type;
      typedef ID id_image_type;
      typedef pgsql::connection connection_type;

      extra_statement_cache_ptr (): p_ (0) {}
      ~extra_statement_cache_ptr ()
      {
        if (p_ != 0)
          (this->*deleter_) (0, 0, 0, 0, 0, 0, 0);
      }

      T&
      get (connection_type& c,
           image_type& im,
           id_image_type& idim,
           binding& id,
           binding* idv,
           native_binding& idn,
           const Oid* idt)
      {
        if (p_ == 0)
          allocate (&c, &im, &idim, &id, (idv != 0 ? idv : &id), &idn, idt);

        return *p_;
      }

    private:
      void
      allocate (connection_type*,
                image_type*, id_image_type*,
                binding*, binding*, native_binding*, const Oid*);

    private:
      T* p_;
      void (extra_statement_cache_ptr::*deleter_) (
        connection_type*,
        image_type*, id_image_type*,
        binding*, binding*, native_binding*, const Oid*);
    };

    template <typename T, typename I, typename ID>
    void extra_statement_cache_ptr<T, I, ID>::
    allocate (connection_type* c,
              image_type* im, id_image_type* idim,
              binding* id, binding* idv, native_binding* idn, const Oid* idt)
    {
      // To reduce object code size, this function acts as both allocator
      // and deleter.
      //
      if (p_ == 0)
      {
        p_ = new T (*c, *im, *idim, *id, *idv, *idn, idt);
        deleter_ = &extra_statement_cache_ptr<T, I, ID>::allocate;
      }
      else
        delete p_;
    }

    //
    // Implementation for objects with object id.
    //

    class LIBODB_PGSQL_EXPORT object_statements_base: public statements_base
    {
    public:
      // Locking.
      //
      void
      lock ()
      {
        assert (!locked_);
        locked_ = true;
      }

      void
      unlock ()
      {
        assert (locked_);
        locked_ = false;
      }

      bool
      locked () const
      {
        return locked_;
      }

      struct auto_unlock
      {
        // Unlocks the statement on construction and re-locks it on
        // destruction.
        //
        auto_unlock (object_statements_base&);
        ~auto_unlock ();

      private:
        auto_unlock (const auto_unlock&);
        auto_unlock& operator= (const auto_unlock&);

      private:
        object_statements_base& s_;
      };

    public:
      virtual
      ~object_statements_base ();

    protected:
      object_statements_base (connection_type& conn)
        : statements_base (conn), locked_ (false)
      {
      }

    protected:
      bool locked_;
    };

    template <typename T, bool optimistic>
    struct optimistic_data;

    template <typename T>
    struct optimistic_data<T, true>
    {
      typedef T object_type;
      typedef object_traits_impl<object_type, id_pgsql> object_traits;

      optimistic_data (bind*, char** nv, int* nl, int* nf,
                       std::size_t skip, unsigned long long* status);

      binding*
      id_image_binding () {return &id_image_binding_;}

      native_binding*
      id_image_native_binding () {return &id_image_native_binding_;}

      const Oid*
      id_image_types ()
      {return object_traits::optimistic_erase_statement_types;}

      // The id + optimistic column binding.
      //
      binding id_image_binding_;
      native_binding id_image_native_binding_;

      details::shared_ptr<delete_statement> erase_;
    };

    template <typename T>
    struct optimistic_data<T, false>
    {
      optimistic_data (bind*, char**, int*, int*,
                       std::size_t, unsigned long long*) {}

      binding*
      id_image_binding () {return 0;}

      native_binding*
      id_image_native_binding () {return 0;}

      const Oid*
      id_image_types () {return 0;}
    };

    template <typename T>
    class object_statements: public object_statements_base
    {
    public:
      typedef T object_type;
      typedef object_traits_impl<object_type, id_pgsql> object_traits;
      typedef typename object_traits::id_type id_type;
      typedef typename object_traits::pointer_type pointer_type;
      typedef typename object_traits::image_type image_type;
      typedef typename object_traits::id_image_type id_image_type;

      typedef
      typename object_traits::pointer_cache_traits
      pointer_cache_traits;

      typedef
      typename object_traits::extra_statement_cache_type
      extra_statement_cache_type;

      typedef pgsql::insert_statement insert_statement_type;
      typedef pgsql::select_statement select_statement_type;
      typedef pgsql::update_statement update_statement_type;
      typedef pgsql::delete_statement delete_statement_type;

      // Automatic lock.
      //
      struct auto_lock
      {
        // Lock the statements unless they are already locked in which
        // case subsequent calls to locked() will return false.
        //
        auto_lock (object_statements&);

        // Unlock the statemens if we are holding the lock and clear
        // the delayed loads. This should only happen in case an
        // exception is thrown. In normal circumstances, the user
        // should call unlock() explicitly.
        //
        ~auto_lock ();

        // Return true if this auto_lock instance holds the lock.
        //
        bool
        locked () const;

        // Unlock the statemens.
        //
        void
        unlock ();

      private:
        auto_lock (const auto_lock&);
        auto_lock& operator= (const auto_lock&);

      private:
        object_statements& s_;
        bool locked_;
      };

    public:
      object_statements (connection_type&);

      virtual
      ~object_statements ();

      // Delayed loading.
      //
      typedef void (*loader_function) (odb::database&,
                                       const id_type&,
                                       object_type&,
                                       const schema_version_migration*);
      void
      delay_load (const id_type& id,
                  object_type& obj,
                  const typename pointer_cache_traits::position_type& p,
                  loader_function l = 0)
      {
        delayed_.push_back (delayed_load (id, obj, p, l));
      }

      void
      load_delayed (const schema_version_migration* svm)
      {
        assert (locked ());

        if (!delayed_.empty ())
          load_delayed_<object_statements> (svm);
      }

      void
      clear_delayed ()
      {
        if (!delayed_.empty ())
          clear_delayed_ ();
      }

      // Object image.
      //
      image_type&
      image (std::size_t i = 0) {return images_[i].obj;}

      // Insert binding.
      //
      std::size_t
      insert_image_version () const { return insert_image_version_;}

      void
      insert_image_version (std::size_t v) {insert_image_version_ = v;}

      binding&
      insert_image_binding () {return insert_image_binding_;}

      // Update binding.
      //
      std::size_t
      update_image_version () const { return update_image_version_;}

      void
      update_image_version (std::size_t v) {update_image_version_ = v;}

      std::size_t
      update_id_image_version () const { return update_id_image_version_;}

      void
      update_id_image_version (std::size_t v) {update_id_image_version_ = v;}

      binding&
      update_image_binding () {return update_image_binding_;}

      // Select binding.
      //
      std::size_t
      select_image_version () const { return select_image_version_;}

      void
      select_image_version (std::size_t v) {select_image_version_ = v;}

      binding&
      select_image_binding () {return select_image_binding_;}

      bool*
      select_image_truncated () {return select_image_truncated_;}

      // Object id image and binding.
      //
      id_image_type&
      id_image (std::size_t i = 0) {return images_[i].id;}

      std::size_t
      id_image_version () const {return id_image_version_;}

      void
      id_image_version (std::size_t v) {id_image_version_ = v;}

      binding&
      id_image_binding () {return id_image_binding_;}

      // Optimistic id + managed column image binding. It points to
      // the same suffix as id binding and they are always updated
      // at the same time.
      //
      binding&
      optimistic_id_image_binding () {return od_.id_image_binding_;}

      // Statements.
      //
      insert_statement_type&
      persist_statement ()
      {
        if (persist_ == 0)
          persist_.reset (
            new (details::shared) insert_statement_type (
              conn_,
              object_traits::persist_statement_name,
              object_traits::persist_statement,
              object_traits::versioned, // Process if versioned.
              object_traits::persist_statement_types,
              insert_column_count,
              insert_image_binding_,
              insert_image_native_binding_,
              (object_traits::auto_id ? &id_image_binding_ : 0),
              false));

        return *persist_;
      }

      select_statement_type&
      find_statement ()
      {
        if (find_ == 0)
          find_.reset (
            new (details::shared) select_statement_type (
              conn_,
              object_traits::find_statement_name,
              object_traits::find_statement,
              object_traits::versioned, // Process if versioned.
              false,                    // Don't optimize.
              object_traits::find_statement_types,
              id_column_count,
              id_image_binding_,
              id_image_native_binding_,
              select_image_binding_,
              false));

        return *find_;
      }

      update_statement_type&
      update_statement ()
      {
        if (update_ == 0)
          update_.reset (
            new (details::shared) update_statement_type (
              conn_,
              object_traits::update_statement_name,
              object_traits::update_statement,
              object_traits::versioned, // Process if versioned.
              object_traits::update_statement_types,
              update_column_count + id_column_count,
              update_image_binding_,
              update_image_native_binding_,
              false));

        return *update_;
      }

      delete_statement_type&
      erase_statement ()
      {
        if (erase_ == 0)
          erase_.reset (
            new (details::shared) delete_statement_type (
              conn_,
              object_traits::erase_statement_name,
              object_traits::erase_statement,
              object_traits::find_statement_types, // The same as find (id).
              id_column_count,
              id_image_binding_,
              id_image_native_binding_,
              false));

        return *erase_;
      }

      delete_statement_type&
      optimistic_erase_statement ()
      {
        if (od_.erase_ == 0)
          od_.erase_.reset (
            new (details::shared) delete_statement_type (
              conn_,
              object_traits::optimistic_erase_statement_name,
              object_traits::optimistic_erase_statement,
              object_traits::optimistic_erase_statement_types,
              id_column_count + managed_optimistic_column_count,
              od_.id_image_binding_,
              od_.id_image_native_binding_,
              false));

        return *od_.erase_;
      }

      // Extra (container, section) statement cache.
      //
      extra_statement_cache_type&
      extra_statement_cache ()
      {
        return extra_statement_cache_.get (
          conn_,
          images_[0].obj,
          images_[0].id,
          id_image_binding_,
          od_.id_image_binding (),
          id_image_native_binding_,
          object_traits::find_statement_types);
      }

    public:
      // select = total - separate_load
      // insert = total - inverse - managed_optimistic - auto_id
      // update = total - inverse - managed_optimistic - id - readonly -
      //  separate_update
      //
      static const std::size_t select_column_count =
        object_traits::column_count -
        object_traits::separate_load_column_count;

      static const std::size_t id_column_count =
        object_traits::id_column_count;

      static const std::size_t insert_column_count =
        object_traits::column_count -
        object_traits::inverse_column_count -
        object_traits::managed_optimistic_column_count -
        (object_traits::auto_id ? id_column_count : 0);

      static const std::size_t update_column_count =
        insert_column_count -
        (object_traits::auto_id ? 0 : id_column_count) -
        object_traits::readonly_column_count -
        object_traits::separate_update_column_count;

      static const std::size_t managed_optimistic_column_count =
        object_traits::managed_optimistic_column_count;

    private:
      object_statements (const object_statements&);
      object_statements& operator= (const object_statements&);

    protected:
      template <typename STS>
      void
      load_delayed_ (const schema_version_migration*);

      void
      clear_delayed_ ();

    protected:
      template <typename T1>
      friend class polymorphic_derived_object_statements;

      extra_statement_cache_ptr<extra_statement_cache_type,
                                image_type,
                                id_image_type> extra_statement_cache_;

      // The UPDATE statement uses both the object and id image. Keep them
      // next to each other so that the same skip distance can be used in
      // batch binding.
      //
      struct images
      {
        image_type obj;
        id_image_type id;
      };

      images images_[object_traits::batch];
      unsigned long long status_[object_traits::batch];

      // Select binding.
      //
      std::size_t select_image_version_;
      binding select_image_binding_;
      bind select_image_bind_[select_column_count];
      bool select_image_truncated_[select_column_count];

      // Insert binding.
      //
      std::size_t insert_image_version_;
      binding insert_image_binding_;
      bind insert_image_bind_[
        insert_column_count != 0 ? insert_column_count : 1];
      native_binding insert_image_native_binding_;
      char* insert_image_values_[
        insert_column_count != 0 ? insert_column_count : 1];
      int insert_image_lengths_[
        insert_column_count != 0 ? insert_column_count : 1];
      int insert_image_formats_[
        insert_column_count != 0 ? insert_column_count : 1];

      // Update binding. Note that the id suffix is bound to id_image_
      // below instead of image_ which makes this binding effectively
      // bound to two images. As a result, we have to track versions
      // for both of them. If this object uses optimistic concurrency,
      // then the binding for the managed column (version, timestamp,
      // etc) comes after the id and the image for such a column is
      // stored as part of the id image.
      //
      std::size_t update_image_version_;
      std::size_t update_id_image_version_;
      binding update_image_binding_;
      bind update_image_bind_[update_column_count + id_column_count +
                              managed_optimistic_column_count];
      native_binding update_image_native_binding_;
      char* update_image_values_[update_column_count + id_column_count +
                                 managed_optimistic_column_count];
      int update_image_lengths_[update_column_count + id_column_count +
                                managed_optimistic_column_count];
      int update_image_formats_[update_column_count + id_column_count +
                                managed_optimistic_column_count];

      // Id image binding (only used as a parameter). Uses the suffix in
      // the update bind.
      //
      std::size_t id_image_version_;
      binding id_image_binding_;
      native_binding id_image_native_binding_;

      // Extra data for objects with optimistic concurrency support.
      //
      optimistic_data<T, managed_optimistic_column_count != 0> od_;

      details::shared_ptr<insert_statement_type> persist_;
      details::shared_ptr<select_statement_type> find_;
      details::shared_ptr<update_statement_type> update_;
      details::shared_ptr<delete_statement_type> erase_;

      // Delayed loading.
      //
      struct delayed_load
      {
        typedef typename pointer_cache_traits::position_type position_type;

        delayed_load () {}
        delayed_load (const id_type& i,
                      object_type& o,
                      const position_type& p,
                      loader_function l)
            : id (i), obj (&o), pos (p), loader (l)
        {
        }

        id_type id;
        object_type* obj;
        position_type pos;
        loader_function loader;
      };

      typedef std::vector<delayed_load> delayed_loads;
      delayed_loads delayed_;

      // Delayed vectors swap guard. See the load_delayed_() function for
      // details.
      //
      struct swap_guard
      {
        swap_guard (object_statements& os, delayed_loads& dl)
            : os_ (os), dl_ (dl)
        {
          dl_.swap (os_.delayed_);
        }

        ~swap_guard ()
        {
          os_.clear_delayed ();
          dl_.swap (os_.delayed_);
        }

      private:
        object_statements& os_;
        delayed_loads& dl_;
      };
    };
  }
}

#include <odb/pgsql/simple-object-statements.ixx>
#include <odb/pgsql/simple-object-statements.txx>

#include <odb/post.hxx>

#endif // ODB_PGSQL_SIMPLE_OBJECT_STATEMENTS_HXX
