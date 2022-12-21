// file      : libcutl/static-ptr.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_STATIC_PTR_HXX
#define LIBCUTL_STATIC_PTR_HXX

#include <cstddef> // std::size_t

namespace cutl
{
  // This class template implements Jerry Schwarz's static
  // initialization technique commonly found in iostream
  // implementations.
  //
  // The second template argument is used to make sure the
  // instantiation of static_ptr is unique.
  //
  template <typename X, typename ID>
  class static_ptr
  {
  public:
    static_ptr ()
    {
      if (count_ == 0)
        x_ = new X;

      ++count_;
    }

    ~static_ptr ()
    {
      if (--count_ == 0)
        delete x_;
    }

  private:
    static_ptr (static_ptr const&);

    static_ptr&
    operator= (static_ptr const&);

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

    X*
    get () const
    {
      return x_;
    }

  private:
    static X* x_;
    static std::size_t count_;
  };

  template <typename X, typename ID>
  X* static_ptr<X, ID>::x_ = 0;

  template <typename X, typename ID>
  std::size_t static_ptr<X, ID>::count_ = 0;
}

#endif // LIBCUTL_STATIC_PTR_HXX
