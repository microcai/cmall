// file      : libbutl/prefix-map.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
{
  template <typename M>
  auto prefix_map_common<M>::
  find_sub (const key_type& k) -> std::pair<iterator, iterator>
  {
    const auto& c (this->key_comp ());

    std::pair<iterator, iterator> r;
    r.first = this->lower_bound (k);

    for (r.second = r.first; r.second != this->end (); ++r.second)
    {
      if (!c.prefix (k, r.second->first))
        break;
    }

    return r;
  }

  template <typename M>
  auto prefix_map_common<M>::
  find_sub (const key_type& k) const ->
    std::pair<const_iterator, const_iterator>
  {
    const auto& c (this->key_comp ());

    std::pair<const_iterator, const_iterator> r;
    r.first = this->lower_bound (k);

    for (r.second = r.first; r.second != this->end (); ++r.second)
    {
      if (!c.prefix (k, r.second->first))
        break;
    }

    return r;
  }

  template <typename M>
  auto prefix_map_common<M>::
  find_sup (const key_type& k) -> iterator
  {
    // There seems to be only two possible algorithms here:
    //
    // 1. Iterate over the key: get progressively outer prefixes and look
    //    for a match in the map.
    //
    // 2. Iterate over entries: get the upper bound for the key and iterate
    //    backwards looking for a prefix.
    //
    // The drawback of the first approach is that we have to create a new key
    // which will most likely involve a memory allocation (we can probably
    // limit it to a single allocation by reusing the key instance).
    //
    // The drawback of the second approch is that we may have a lot of entries
    // between the lower bound and the prefix (in contrast, keys normally only
    // have a handful of components).
    //
    // Tests have shown that (2) can be significantly faster than (1).
    //
#if 0
    const auto& c (this->key_comp ());

    for (auto i (this->upper_bound (k)), b (this->begin ()); i != b; )
    {
      --i;
      if (c.prefix (i->first, k))
        return i;
    }

    return this->end ();
#else
    // First look for the exact match before making any copies.
    //
    auto i (this->find (k)), e (this->end ());

    if (i == e)
    {
      const auto& c (this->key_comp ());

      for (key_type p (k); c.prefix (p); )
      {
        i = this->find (p);
        if (i != e)
          break;
      }
    }

    return i;
#endif
  }

  template <typename M>
  auto prefix_map_common<M>::
  find_sup (const key_type& k) const -> const_iterator
  {
#if 0
    const auto& c (this->key_comp ());

    for (auto i (this->upper_bound (k)), b (this->begin ()); i != b; )
    {
      --i;
      if (c.prefix (i->first, k))
        return i;
    }

    return this->end ();
#else
    auto i (this->find (k)), e (this->end ());

    if (i == e)
    {
      const auto& c (this->key_comp ());

      for (key_type p (k); c.prefix (p); )
      {
        i = this->find (p);
        if (i != e)
          break;
      }
    }

    return i;
#endif
  }
}
