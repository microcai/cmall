// file      : libcutl/compiler/context.txx
// license   : MIT; see accompanying LICENSE file

namespace cutl
{
  namespace compiler
  {
    template <typename X>
    X& context::
    get (std::string const& key)
    {
      map::iterator i (map_.find (key));

      if (i == map_.end ())
        throw no_entry ();

      try
      {
        return i->second. template value<X> ();
      }
      catch (container::any::typing const&)
      {
        throw typing ();
      }
    }

    template <typename X>
    X const& context::
    get (std::string const& key) const
    {
      map::const_iterator i (map_.find (key));

      if (i == map_.end ())
        throw no_entry ();

      try
      {
        return i->second. template value<X> ();
      }
      catch (container::any::typing const&)
      {
        throw typing ();
      }
    }

    template <typename X>
    X const& context::
    get (std::string const& key, X const& default_value) const
    {
      map::const_iterator i (map_.find (key));

      if (i == map_.end ())
        return default_value;

      try
      {
        return i->second. template value<X> ();
      }
      catch (container::any::typing const&)
      {
        throw typing ();
      }
    }

    template <typename X>
    X& context::
    set (std::string const& key, X const& value)
    {
      try
      {
        std::pair<map::iterator, bool> r (
          map_.insert (map::value_type (key, value)));

        X& x (r.first->second. template value<X> ());

        if (!r.second)
          x = value;

        return x;
      }
      catch (container::any::typing const&)
      {
        throw typing ();
      }
    }
  }
}
