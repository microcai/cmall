// file      : odb/pgsql/polymorphic-object-statements.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_PGSQL_POLYMORPHIC_OBJECT_STATEMENTS_HXX
#define ODB_PGSQL_POLYMORPHIC_OBJECT_STATEMENTS_HXX

#include <odb/pre.hxx>

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
#include <odb/pgsql/simple-object-statements.hxx>

namespace odb
{
  namespace pgsql
  {
    //
    // Implementation for polymorphic objects.
    //

    template <typename T>
    class polymorphic_root_object_statements: public object_statements<T>
    {
    public:
      typedef typename object_statements<T>::connection_type connection_type;
      typedef typename object_statements<T>::object_traits object_traits;
      typedef typename object_statements<T>::id_image_type id_image_type;

      typedef
      typename object_traits::discriminator_image_type
      discriminator_image_type;

      typedef
      typename object_statements<T>::select_statement_type
      select_statement_type;

    public:
      // Interface compatibility with derived_object_statements.
      //
      typedef polymorphic_root_object_statements root_statements_type;

      root_statements_type&
      root_statements ()
      {
        return *this;
      }

    public:
      // Discriminator binding.
      //
      discriminator_image_type&
      discriminator_image () {return discriminator_image_;}

      std::size_t
      discriminator_image_version () const
      {return discriminator_image_version_;}

      void
      discriminator_image_version (std::size_t v)
      {discriminator_image_version_ = v;}

      binding&
      discriminator_image_binding () {return discriminator_image_binding_;}

      bool*
      discriminator_image_truncated () {return discriminator_image_truncated_;}

      // Id binding for discriminator retrieval.
      //
      id_image_type&
      discriminator_id_image () {return discriminator_id_image_;}

      std::size_t
      discriminator_id_image_version () const
      {return discriminator_id_image_version_;}

      void
      discriminator_id_image_version (std::size_t v)
      {discriminator_id_image_version_ = v;}

      binding&
      discriminator_id_image_binding ()
      {return discriminator_id_image_binding_;}

      // Expose id native binding from object_statements.
      //
      native_binding&
      id_image_native_binding () {return this->id_image_native_binding_;}

      //
      //
      select_statement_type&
      find_discriminator_statement ()
      {
        if (find_discriminator_ == 0)
          find_discriminator_.reset (
            new (details::shared) select_statement_type (
              this->conn_,
              object_traits::find_discriminator_statement_name,
              object_traits::find_discriminator_statement,
              false, // Doesn't need to be processed.
              false, // Don't optimize.
              object_traits::find_statement_types, // The same as find (id).
              id_column_count,
              discriminator_id_image_binding_,
              discriminator_id_image_native_binding_,
              discriminator_image_binding_,
              false));

        return *find_discriminator_;
      }

    public:
      polymorphic_root_object_statements (connection_type&);

      virtual
      ~polymorphic_root_object_statements ();

      // Static "override" (statements type).
      //
      void
      load_delayed (const schema_version_migration* svm)
      {
        assert (this->locked ());

        if (!this->delayed_.empty ())
          this->template load_delayed_<polymorphic_root_object_statements> (
            svm);
      }

    public:
      static const std::size_t id_column_count =
        object_statements<T>::id_column_count;

      static const std::size_t discriminator_column_count =
        object_traits::discriminator_column_count;

      static const std::size_t managed_optimistic_column_count =
        object_traits::managed_optimistic_column_count;

    private:
      // Discriminator image.
      //
      discriminator_image_type discriminator_image_;
      std::size_t discriminator_image_version_;
      binding discriminator_image_binding_;
      bind discriminator_image_bind_[discriminator_column_count +
                                     managed_optimistic_column_count];
      bool discriminator_image_truncated_[discriminator_column_count +
                                          managed_optimistic_column_count];

      // Id image for discriminator retrieval (only used as a parameter).
      //
      id_image_type discriminator_id_image_;
      std::size_t discriminator_id_image_version_;
      binding discriminator_id_image_binding_;
      native_binding discriminator_id_image_native_binding_;
      bind discriminator_id_image_bind_[id_column_count];
      char* discriminator_id_image_values_[id_column_count];
      int discriminator_id_image_lengths_[id_column_count];
      int discriminator_id_image_formats_[id_column_count];

      details::shared_ptr<select_statement_type> find_discriminator_;
    };

    template <typename T>
    class polymorphic_derived_object_statements: public statements_base
    {
    public:
      typedef T object_type;
      typedef object_traits_impl<object_type, id_pgsql> object_traits;
      typedef typename object_traits::id_type id_type;
      typedef typename object_traits::pointer_type pointer_type;
      typedef typename object_traits::id_image_type id_image_type;
      typedef typename object_traits::image_type image_type;

      typedef typename object_traits::root_type root_type;
      typedef
      polymorphic_root_object_statements<root_type>
      root_statements_type;

      typedef typename object_traits::base_type base_type;
      typedef
      typename object_traits::base_traits::statements_type
      base_statements_type;

      typedef
      typename object_traits::extra_statement_cache_type
      extra_statement_cache_type;

      typedef pgsql::insert_statement insert_statement_type;
      typedef pgsql::select_statement select_statement_type;
      typedef pgsql::update_statement update_statement_type;
      typedef pgsql::delete_statement delete_statement_type;

      typedef typename root_statements_type::auto_lock auto_lock;

    public:
      polymorphic_derived_object_statements (connection_type&);

      virtual
      ~polymorphic_derived_object_statements ();

    public:
      // Delayed loading.
      //
      static void
      delayed_loader (odb::database&,
                      const id_type&,
                      root_type&,
                      const schema_version_migration*);
    public:
      // Root and immediate base statements.
      //
      root_statements_type&
      root_statements ()
      {
        return root_statements_;
      }

      base_statements_type&
      base_statements ()
      {
        return base_statements_;
      }

    public:
      // Object image.
      //
      image_type&
      image ()
      {
        return image_;
      }

      // Insert binding.
      //
      std::size_t
      insert_image_version () const { return insert_image_version_;}

      void
      insert_image_version (std::size_t v) {insert_image_version_ = v;}

      std::size_t
      insert_id_binding_version () const { return insert_id_binding_version_;}

      void
      insert_id_binding_version (std::size_t v) {insert_id_binding_version_ = v;}

      binding&
      insert_image_binding () {return insert_image_binding_;}

      // Update binding.
      //
      std::size_t
      update_image_version () const { return update_image_version_;}

      void
      update_image_version (std::size_t v) {update_image_version_ = v;}

      std::size_t
      update_id_binding_version () const { return update_id_binding_version_;}

      void
      update_id_binding_version (std::size_t v) {update_id_binding_version_ = v;}

      binding&
      update_image_binding () {return update_image_binding_;}

      // Select binding.
      //
      std::size_t*
      select_image_versions () { return select_image_versions_;}

      binding*
      select_image_bindings () {return select_image_bindings_;}

      binding&
      select_image_binding (std::size_t d)
      {
        return select_image_bindings_[object_traits::depth - d];
      }

      bool*
      select_image_truncated () {return select_image_truncated_;}

      // Object id binding (comes from the root statements).
      //
      id_image_type&
      id_image () {return root_statements_.id_image ();}

      std::size_t
      id_image_version () const {return root_statements_.id_image_version ();}

      void
      id_image_version (std::size_t v) {root_statements_.id_image_version (v);}

      binding&
      id_image_binding () {return root_statements_.id_image_binding ();}

      binding&
      optimistic_id_image_binding () {
        return root_statements_.optimistic_id_image_binding ();}

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
              0,
              false));

        return *persist_;
      }

      select_statement_type&
      find_statement (std::size_t d)
      {
        std::size_t i (object_traits::depth - d);
        details::shared_ptr<select_statement_type>& p (find_[i]);

        if (p == 0)
          p.reset (
            new (details::shared) select_statement_type (
              conn_,
              object_traits::find_statement_names[i],
              object_traits::find_statements[i],
              object_traits::versioned, // Process if versioned.
              false,                    // Don't optimize.
              object_traits::find_statement_types,
              id_column_count,
              root_statements_.id_image_binding (),
              root_statements_.id_image_native_binding (),
              select_image_bindings_[i],
              false));

        return *p;
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
              root_statements_.id_image_binding (),
              root_statements_.id_image_native_binding (),
              false));

        return *erase_;
      }

      // Extra (container, section) statement cache.
      //
      extra_statement_cache_type&
      extra_statement_cache ()
      {
        return extra_statement_cache_.get (
          conn_,
          image_,
          id_image (),
          id_image_binding (),
          &id_image_binding (), // Note, not id+version.
          root_statements_.id_image_native_binding (),
          object_traits::find_statement_types);
      }

    public:
      // select = total - id - separate_load + base::select
      // insert = total - inverse
      // update = total - inverse - id - readonly - separate_update
      //
      static const std::size_t id_column_count =
        object_traits::id_column_count;

      static const std::size_t select_column_count =
        object_traits::column_count -
        id_column_count -
        object_traits::separate_load_column_count +
        base_statements_type::select_column_count;

      static const std::size_t insert_column_count =
        object_traits::column_count -
        object_traits::inverse_column_count;

      static const std::size_t update_column_count =
        insert_column_count -
        object_traits::id_column_count -
        object_traits::readonly_column_count -
        object_traits::separate_update_column_count;

    private:
      polymorphic_derived_object_statements (
        const polymorphic_derived_object_statements&);

      polymorphic_derived_object_statements&
      operator= (const polymorphic_derived_object_statements&);

    private:
      root_statements_type& root_statements_;
      base_statements_type& base_statements_;

      extra_statement_cache_ptr<extra_statement_cache_type,
                                image_type,
                                id_image_type> extra_statement_cache_;

      image_type image_;

      // Select binding. Here we are have an array of statements/bindings
      // one for each depth. In other words, if we have classes root, base,
      // and derived, then we have the following array of statements:
      //
      // [0] d + b + r
      // [1] d + b
      // [2] d
      //
      // Also, because we have a chain of images bound to these statements,
      // we have an array of versions, one entry for each base plus one for
      // our own image.
      //
      // A poly-abstract class only needs the first statement and in this
      // case we have only one entry in the the bindings and statements
      // arrays (but not versions; we still have a chain of images).
      //
      std::size_t select_image_versions_[object_traits::depth];
      binding select_image_bindings_[
        object_traits::abstract ? 1 : object_traits::depth];
      bind select_image_bind_[select_column_count];
      bool select_image_truncated_[select_column_count];

      // Insert binding. The id binding is copied from the hierarchy root.
      //
      std::size_t insert_image_version_;
      std::size_t insert_id_binding_version_;
      binding insert_image_binding_;
      bind insert_image_bind_[insert_column_count];
      native_binding insert_image_native_binding_;
      char* insert_image_values_[insert_column_count];
      int insert_image_lengths_[insert_column_count];
      int insert_image_formats_[insert_column_count];

      // Update binding. The id suffix binding is copied from the hierarchy
      // root.
      //
      std::size_t update_image_version_;
      std::size_t update_id_binding_version_;
      binding update_image_binding_;
      bind update_image_bind_[update_column_count + id_column_count];
      native_binding update_image_native_binding_;
      char* update_image_values_[update_column_count + id_column_count];
      int update_image_lengths_[update_column_count + id_column_count];
      int update_image_formats_[update_column_count + id_column_count];

      details::shared_ptr<insert_statement_type> persist_;
      details::shared_ptr<select_statement_type> find_[
        object_traits::abstract ? 1 : object_traits::depth];
      details::shared_ptr<update_statement_type> update_;
      details::shared_ptr<delete_statement_type> erase_;
    };
  }
}

#include <odb/pgsql/polymorphic-object-statements.txx>

#include <odb/post.hxx>

#endif // ODB_PGSQL_POLYMORPHIC_OBJECT_STATEMENTS_HXX
