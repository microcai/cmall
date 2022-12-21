// file      : libcutl/compiler/type-id.ixx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    inline
    type_id::
    type_id (std::type_info const& ti)
        : ti_ (&ti)
    {
    }

    inline
    char const* type_id::
    name () const
    {
      return ti_->name ();
    }

    inline
    bool
    operator== (type_id const& x, type_id const& y)
    {
      return *x.ti_ == *y.ti_;
    }

    inline
    bool
    operator!= (type_id const& x, type_id const& y)
    {
      return *x.ti_ != *y.ti_;
    }

    inline
    bool
    operator< (type_id const& x, type_id const& y)
    {
      return x.ti_->before (*y.ti_);
    }
  }
}
