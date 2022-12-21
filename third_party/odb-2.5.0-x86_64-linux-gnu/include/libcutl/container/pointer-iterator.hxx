// file      : libcutl/container/pointer-iterator.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_CONTAINER_POINTER_ITERATOR_HXX
#define LIBCUTL_CONTAINER_POINTER_ITERATOR_HXX

#include <iterator> // std::iterator_traits

#include <libcutl/meta/remove-p.hxx>

namespace cutl
{
  namespace container
  {
    template <typename I>
    class pointer_iterator
    {
    public:
      typedef
      typename meta::remove_p<typename std::iterator_traits<I>::value_type>::r
      value_type;

      typedef
      typename std::iterator_traits<I>::iterator_category
      iterator_category;

      typedef
      typename std::iterator_traits<I>::difference_type
      difference_type;

      typedef value_type& reference;
      typedef value_type* pointer;
      typedef I base_iterator;

    public:
      pointer_iterator ()
          : i_ () // I can be of a pointer type.
      {
      }

      pointer_iterator (I const& i)
          : i_ (i)
      {
      }

    public:
      reference
      operator* () const
      {
        return **i_;
      }

      pointer
      operator-> () const
      {
        return *i_;
      }

      I const&
      base () const
      {
        return i_;
      }

    public:
      // Forward iterator requirements.
      //
      pointer_iterator&
      operator++ ()
      {
        ++i_;
        return *this;
      }

      pointer_iterator
      operator++ (int)
      {
        pointer_iterator r (*this);
        ++i_;
        return r;
      }

      pointer_iterator&
      operator-- ()
      {
        --i_;
        return *this;
      }

      pointer_iterator
      operator-- (int)
      {
        pointer_iterator r (*this);
        --i_;
        return r;
      }

    private:
      I i_;
    };

    template <typename I>
    inline bool
    operator== (pointer_iterator<I> const& a, pointer_iterator<I> const& b)
    {
      return a.base () == b.base ();
    }

    template <typename I>
    inline bool
    operator!= (pointer_iterator<I> const& a, pointer_iterator<I> const& b)
    {
      return a.base () != b.base ();
    }

    template <typename I>
    inline typename pointer_iterator<I>::difference_type
    operator- (pointer_iterator<I> const& a, pointer_iterator<I> const& b)
    {
      return a.base () - b.base ();
    }
  }
}

#endif // LIBCUTL_CONTAINER_POINTER_ITERATOR_HXX
