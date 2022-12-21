// file      : libcutl/compiler/type-info.ixx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    // base_info
    //

    inline
    base_info::
    base_info (type_id const& type_id)
        : type_id_ (type_id), type_info_ (0)
    {
    }

    inline
    type_info_t const& base_info::
    type_info () const
    {
      // We need to do delayed lookup because of the unpredictable
      // order in which type information may be added.
      //
      // @@ MT-unsafe
      //
      if (type_info_ == 0)
        type_info_ = &(lookup (type_id_));

      return *type_info_;
    }

    // type_info
    //

    inline
    type_info::
    type_info (type_id_t const& tid)
        : type_id_ (tid)
    {
    }

    inline
    type_id_t type_info::
    type_id () const
    {
      return type_id_;
    }

    inline
    type_info::base_iterator type_info::
    begin_base () const
    {
      return bases_.begin ();
    }


    inline
    type_info::base_iterator type_info::
    end_base () const
    {
      return bases_.end ();
    }

    inline
    void type_info::
    add_base (type_id_t const& tid)
    {
      bases_.push_back (base_info (tid));
    }

    //
    //
    inline type_info const&
    lookup (std::type_info const& tid)
    {
      return lookup (type_id (tid));
    }

    template <typename X>
    inline type_info const&
    lookup (X const volatile& x)
    {
      return lookup (typeid (x));
    }

    template<typename X>
    inline type_info const&
    lookup ()
    {
      return lookup (typeid (X));
    }
  }
}
