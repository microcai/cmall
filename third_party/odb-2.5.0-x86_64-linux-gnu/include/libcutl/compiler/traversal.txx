// file      : libcutl/compiler/traversal.txx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    // traverser
    //
    template<typename B>
    traverser<B>::
    ~traverser ()
    {
    }

    // traverser_impl
    //

    template <typename X, typename B>
    void traverser_impl<X, B>::
    trampoline (B& x)
    {
      this->traverse (dynamic_cast<type&> (x));
    }

    // dispatcher
    //

    template <typename B>
    dispatcher<B>::
    ~dispatcher ()
    {
    }

    template <typename B>
    void dispatcher<B>::
    traverser (traverser_map<B>& m)
    {
      // Copy entries from m to our map.
      //
      for (typename traverser_map<B>::iterator
             i (m.begin ()), e (m.end ()); i != e; ++i)
      {
        typename traverser_map<B>::traversers& travs (this->map_[i->first]);

        for (typename traverser_map<B>::traversers::const_iterator
               t (i->second.begin ()), e (i->second.end ()); t != e; ++t)
        {
          travs.push_back (*t);
        }
      }
    }

    template <typename B>
    void dispatcher<B>::
    dispatch (B& x)
    {
      using std::size_t;

      level_map levels;
      type_info const& ti (lookup (x));
      size_t max (compute_levels (ti, 0, levels));

      // cerr << "starting dispatch process for " << ti.type_id ().name ()
      //      << " with " << max << " levels" << endl;

      for (size_t l (0); l < max + 1; ++l)
      {
        type_info_set dispatched;

        for (typename level_map::const_iterator
               i (levels.begin ()), e (levels.end ()); i != e; ++i)
        {
          if (i->second == l)
          {
            typename traverser_map<B>::map_type::const_iterator v (
              this->map_.find (i->first.type_id ()));

            if (v != this->map_.end ())
            {
              // cerr << "dispatching traversers for " << ti.type_id ().name ()
              //      << " as " << i->first.type_id ().name () << endl;

              typename traverser_map<B>::traversers const& travs (v->second);

              for (typename traverser_map<B>::traversers::const_iterator
                     ti (travs.begin ()), te (travs.end ()); ti != te; ++ti)
              {
                (*ti)->trampoline (x);
              }

              flatten_tree (i->first, dispatched);
            }
          }
        }

        // Remove traversed types from the level map.
        //
        for (typename type_info_set::const_iterator i (dispatched.begin ());
             i != dispatched.end (); ++i)
        {
          levels.erase (*i);
        }
      }
    }

    template <typename B>
    std::size_t dispatcher<B>::
    compute_levels (type_info const& ti, std::size_t cur, level_map& map)
    {
      using std::size_t;

      size_t ret (cur);

      if (map.find (ti) == map.end () || map[ti] < cur)
        map[ti] = cur;

      for (type_info::base_iterator i (ti.begin_base ());
           i != ti.end_base (); ++i)
      {
        size_t tmp (compute_levels (i->type_info (), cur + 1, map));

        if (tmp > ret)
          ret = tmp;
      }

      return ret;
    }

    template <typename B>
    void dispatcher<B>::
    flatten_tree (type_info const& ti, type_info_set& set)
    {
      set.insert (ti);

      for (type_info::base_iterator i (ti.begin_base ());
           i != ti.end_base (); ++i)
      {
        flatten_tree (i->type_info (), set);
      }
    }
  }
}
