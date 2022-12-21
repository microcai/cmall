// file      : libcutl/shared-ptr.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_SHARED_PTR_HXX
#define LIBCUTL_SHARED_PTR_HXX

#include <libcutl/shared-ptr/base.hxx>

namespace cutl
{
  template <typename X>
  class shared_ptr: bits::counter_ops<typename bits::counter_type<X>::r, X>
  {
    typedef bits::counter_ops<typename bits::counter_type<X>::r, X> base;

  public:
    ~shared_ptr ()
    {
      if (x_ != 0)
        base::dec (x_);
    }

    explicit
    shared_ptr (X* x = 0)
        : base (x), x_ (x)
    {
    }

    shared_ptr (shared_ptr const& x)
        : base (x), x_ (x.x_)
    {
      if (x_ != 0)
        base::inc (x_);
    }

    template <typename Y>
    shared_ptr (shared_ptr<Y> const& x)
        : base (x), x_ (x.x_)
    {
      if (x_ != 0)
        base::inc (x_);
    }

    shared_ptr&
    operator= (shared_ptr const& x)
    {
      if (x_ != x.x_)
      {
        if (x_ != 0)
          base::dec (x_);

        static_cast<base&> (*this) = x;
        x_ = x.x_;

        if (x_ != 0)
          base::inc (x_);
      }

      return *this;
    }

    template <typename Y>
    shared_ptr&
    operator= (shared_ptr<Y> const& x)
    {
      if (x_ != x.x_)
      {
        if (x_ != 0)
          base::dec (x_);

        static_cast<base&> (*this) = x;
        x_ = x.x_;

        if (x_ != 0)
          base::inc (x_);
      }

      return *this;
    }

  public:
    X*
    operator-> () const
    {
      return x_;
    }

    X&
    operator* () const
    {
      return *x_;
    }

    // Conversion to bool.
    //
    typedef void (shared_ptr::*boolean_convertible)();
    void true_value () {};

    operator boolean_convertible () const
    {
      return x_ ? &shared_ptr<X>::true_value : 0;
    }

  public:
    X*
    get () const
    {
      return x_;
    }

    X*
    release ()
    {
      X* r (x_);
      x_ = 0;
      return r;
    }

    void
    reset (X* x = 0)
    {
      if (x_ != 0)
        base::dec (x_);

      base::reset (x);
      x_ = x;
    }

    std::size_t
    count () const
    {
      return x_ != 0 ? base::count (x_) : 0;
    }

  private:
    template <typename>
    friend class shared_ptr;

    X* x_;
  };

  template <typename X, typename Y>
  inline bool
  operator== (const shared_ptr<X>& x, const shared_ptr<Y>& y)
  {
    return x.get () == y.get ();
  }

  template <typename X, typename Y>
  inline bool
  operator!= (const shared_ptr<X>& x, const shared_ptr<Y>& y)
  {
    return x.get () != y.get ();
  }
}

#endif // LIBCUTL_SHARED_PTR_HXX
