// file      : libcutl/container/map-iterator.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_CONTAINER_MAP_ITERATOR_HXX
#define LIBCUTL_CONTAINER_MAP_ITERATOR_HXX

namespace cutl
{
  namespace container
  {
    // Map iterator adapter that can be used to implement multi-index
    // containers, as discussed in the following post:
    //
    // http://www.codesynthesis.com/~boris/blog/2012/09/11/emulating-boost-multi-index-with-std-containers/
    //
    template <typename M>
    struct map_iterator: M::iterator
    {
      typedef typename M::iterator base_iterator;
      typedef typename M::value_type::second_type value_type;
      typedef value_type* pointer;
      typedef value_type& reference;

      map_iterator () {}
      map_iterator (base_iterator i): base_iterator (i) {}

      reference
      operator* () const
      {
        return base_iterator::operator* ().second;
      }

      pointer
      operator-> () const
      {
        return &base_iterator::operator-> ()->second;
      }
    };

    template <typename M>
    struct map_const_iterator: M::const_iterator
    {
      typedef typename M::iterator base_iterator;
      typedef typename M::const_iterator base_const_iterator;
      typedef const typename M::value_type::second_type value_type;
      typedef value_type* pointer;
      typedef value_type& reference;

      map_const_iterator () {}
      map_const_iterator (base_iterator i): base_const_iterator (i) {}
      map_const_iterator (base_const_iterator i): base_const_iterator (i) {}

      reference
      operator* () const
      {
        return base_const_iterator::operator* ().second;
      }

      pointer
      operator-> () const
      {
        return &base_const_iterator::operator-> ()->second;
      }
    };
  }
}

#endif // LIBCUTL_CONTAINER_MAP_ITERATOR_HXX
