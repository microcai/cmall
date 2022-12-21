// file      : libcutl/shared-ptr/base.txx
// license   : MIT; see accompanying LICENSE file

#include <libcutl/meta/answer.hxx>
#include <libcutl/meta/polymorphic-p.hxx>

namespace cutl
{
  namespace bits
  {
    // Support for locating the counter in the memory block.
    //
    template <typename X, bool poly = meta::polymorphic_p<X>::r>
    struct locator;

    template <typename X>
    struct locator<X, false>
    {
      static std::size_t*
      counter (X* x)
      {
        std::size_t* p (reinterpret_cast<std::size_t*> (x));

        if (*(--p) != 0xDEADBEEF)
          throw not_shared ();

        return --p;
      }
    };

    template <typename X>
    struct locator<X, true>
    {
      static std::size_t*
      counter (X* x)
      {
        std::size_t* p (
          static_cast<std::size_t*> (
            dynamic_cast<void*> (x)));

        if (*(--p) != 0xDEADBEEF)
          throw not_shared ();

        return --p;
      }
    };

    template <typename X>
    std::size_t*
    counter (X const* p)
    {
      return bits::locator<X>::counter (const_cast<X*> (p));
    }

    // Counter type and operations.
    //
    meta::no test (...);
    meta::yes test (shared_base*);

    template <typename X,
              std::size_t A = sizeof (bits::test (reinterpret_cast<X*> (0)))>
    struct counter_type;

    template <typename X>
    struct counter_type<X, sizeof (meta::no)>
    {
      typedef X r;
    };

    template <typename X>
    struct counter_type<X, sizeof (meta::yes)>
    {
      typedef shared_base r;
    };

    template <typename X, typename Y>
    struct counter_ops;

    template <typename X>
    struct counter_ops<X, X>
    {
      counter_ops (X const* p) : counter_ (p ? bits::counter (p) : 0) {}
      counter_ops (counter_ops const& x) : counter_ (x.counter_) {}

      template <typename Z>
      counter_ops (counter_ops<Z, Z> const& x) : counter_ (x.counter_) {}

      counter_ops&
      operator= (counter_ops const& x)
      {
        counter_ = x.counter_;
        return *this;
      }

      template <typename Z>
      counter_ops&
      operator= (counter_ops<Z, Z> const& x)
      {
        counter_ = x.counter_;
        return *this;
      }

      void
      reset (X const* p)
      {
        counter_ = p ? bits::counter (p) : 0;
      }

      void
      inc (X*)
      {
        (*counter_)++;
      }

      void
      dec (X* p)
      {
        if (--(*counter_) == 0)
        {
          p->~X ();
          operator delete (counter_); // Counter is the top of the memory block.
        }
      }

      std::size_t
      count (X const*) const
      {
        return *counter_;
      }

      std::size_t* counter_;
    };

    template <typename Y>
    struct counter_ops<shared_base, Y>
    {
      counter_ops (Y const*) {}
      counter_ops (counter_ops const&) {}

      template <typename Z>
      counter_ops (counter_ops<shared_base, Z> const&) {}

      counter_ops&
      operator= (counter_ops const&)
      {
        return *this;
      }

      template <typename Z>
      counter_ops&
      operator= (counter_ops<shared_base, Z> const&)
      {
        return *this;
      }

      void
      reset (Y const*) {}

      void
      inc (shared_base* p) {p->_inc_ref ();}

      void
      dec (Y* p)
      {
        if (static_cast<shared_base*> (p)->_dec_ref ())
          delete p;
      }

      std::size_t
      count (shared_base const* p) const {return p->_ref_count ();}
    };
  }

  template <typename X>
  inline X*
  inc_ref (X* p)
  {
    bits::counter_ops<typename bits::counter_type<X>::r, X> c (p);
    c.inc (p);
    return p;
  }

  template <typename X>
  inline void
  dec_ref (X* p)
  {
    bits::counter_ops<typename bits::counter_type<X>::r, X> c (p);
    c.dec (p);
  }

  template <typename X>
  inline std::size_t
  ref_count (X const* p)
  {
    bits::counter_ops<typename bits::counter_type<X>::r, X> c (p);
    return c.count (p);
  }
}
