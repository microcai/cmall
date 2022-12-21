// file      : libcutl/container/key.hxx
// license   : MIT; see accompkeying LICENSE file

#ifndef LIBCUTL_CONTAINER_KEY_HXX
#define LIBCUTL_CONTAINER_KEY_HXX

namespace cutl
{
  namespace container
  {
    // A modifiable map key wrapper that can be used to implement multi-
    // index containers, as discussed in the following post:
    //
    // http://www.codesynthesis.com/~boris/blog/2012/09/11/emulating-boost-multi-index-with-std-containers/
    //
    template <class T1, class T2 = void, class T3 = void>
    struct key;

    template <class T1>
    struct key<T1, void, void>
    {
      mutable const T1* p1;

      key (): p1 (0) {}
      key (const T1& r1): p1 (&r1) {}
      void assign (const T1& r1) const {p1 = &r1;}

      bool operator< (const key& x) const {return *p1 < *x.p1;}
    };

    template <class T1, class T2>
    struct key<T1, T2, void>
    {
      mutable const T1* p1;
      mutable const T2* p2;

      key (): p1 (0), p2 (0) {}
      key (const T1& r1, const T2& r2): p1 (&r1), p2 (&r2) {}
      void assign (const T1& r1, const T2& r2) const {p1 = &r1; p2 = &r2;}

      bool operator< (const key& x) const
      {
        return *p1 < *x.p1 || (!(*x.p1 < *p1) && *p2 < *x.p2);
      }
    };

    template <class T1, class T2, class T3>
    struct key
    {
      mutable const T1* p1;
      mutable const T2* p2;
      mutable const T3* p3;

      key (): p1 (0), p2 (0), p3 (0) {}
      key (const T1& r1, const T2& r2, const T3& r3)
          : p1 (&r1), p2 (&r2) , p3 (&r3) {}
      void assign (const T1& r1, const T2& r2, const T3& r3) const
      {p1 = &r1; p2 = &r2; p3 = &r3;}

      bool operator< (const key& x) const
      {
        return (*p1 < *x.p1 ||
                (!(*x.p1 < *p1) && (*p2 < *x.p2 ||
                                    (!(*x.p2 < *p2) && *p3 < *x.p3))));
      }
    };
  }
}

#endif // LIBCUTL_CONTAINER_KEY_HXX
