// file      : odb/nested-container.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_NESTED_CONTAINER_HXX
#define ODB_NESTED_CONTAINER_HXX

#include <odb/pre.hxx>

#include <cstddef> // size_t

#include <odb/forward.hxx>
#include <odb/details/config.hxx> // ODB_CXX11

#ifndef ODB_CXX11
#  error nested container support is only available in C++11
#endif

namespace odb
{
  // Nested container emulation support for ODB.
  //
  // Note that the outer key in the inner container should strictly
  // speaking be a foreign key pointing to the key of the outer
  // container. The only way to achieve this currently is to manually
  // add the constraint via ALTER TABLE ADD CONSTRAINT. Note, however,
  // that as long as we only modify these tables via the ODB container
  // interface, not having the foreign key (and not having ON DELETE
  // CASCADE) should be harmless (since we have a foreign key pointing
  // to the object id).
  //

  // Map key that is used to emulate nested container mapping in ODB.
  // Template parameter T is a tag that allows us to distinguish keys
  // for unrelated containers in order to assign column names, etc.
  // Use inner container type for T.
  //
  template <typename T,
            typename O = std::size_t,
            typename I = std::size_t>
  struct nested_key
  {
    using outer_type = O;
    using inner_type = I;

    outer_type outer;
    inner_type inner;

    nested_key () = default;
    nested_key (outer_type o, inner_type i): outer (o), inner (i) {}

    bool
    operator< (const nested_key& v) const
    {
      return outer < v.outer || (outer == v.outer && inner < v.inner);
    }
  };
}

#include <map>
#include <vector>
#include <cstddef>     // size_t
#include <utility>     // move(), declval()
#include <cassert>
#include <type_traits> // remove_reference

namespace odb
{
  // vector<vector<>>
  //
  template <typename IC>
  struct nested_value_type:
    std::remove_reference<decltype (std::declval<IC> ()[0])> {};

  template <typename IC>
  std::map<nested_key<IC>, typename nested_value_type<IC>::type>
  nested_get (const std::vector<IC>& v)
  {
    using namespace std;

    using I = typename nested_value_type<IC>::type;
    using K = nested_key<IC>;

    map<K, I> r;
    for (size_t n (0); n != v.size (); ++n)
    {
      const IC& o (v[n]);
      for (size_t m (0); m != o.size (); ++m)
        r.emplace (K (n, m), o[m]);
    }
    return r;
  }

  template <typename K, typename I, typename IC>
  void
  nested_set (std::vector<IC>& v, std::map<K, I>&& r)
  {
    using namespace std;

    for (auto& p: r)
    {
      size_t n (p.first.outer);
      size_t m (p.first.inner);
      I& i (p.second);

      assert (n < v.size ());
      assert (m == v[n].size ());
      v[n].push_back (move (i));
    }
  }
}

#include <odb/post.hxx>

#endif // ODB_NESTED_CONTAINER_HXX
