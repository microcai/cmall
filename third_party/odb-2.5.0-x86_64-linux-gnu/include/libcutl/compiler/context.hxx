// file      : libcutl/compiler/context.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_COMPILER_CONTEXT_HXX
#define LIBCUTL_COMPILER_CONTEXT_HXX

#include <map>
#include <string>
#include <cstddef> // std::size_t
#include <typeinfo>

#include <libcutl/exception.hxx>
#include <libcutl/container/any.hxx>

#include <libcutl/export.hxx>

namespace cutl
{
  namespace compiler
  {
    class LIBCUTL_EXPORT context
    {
    public:
      struct no_entry: exception {};
      struct typing: exception {};

    public:
      context () {}

      void
      swap (context& c)
      {
        map_.swap (c.map_);
      }

    private:
      context (context const&);

      context&
      operator= (context const&);

    public:
      std::size_t
      count (char const* key) const
      {
        return count (std::string (key));
      }

      std::size_t
      count (std::string const& key) const
      {
        return map_.count (key);
      }

      template <typename X>
      X&
      get (char const* key)
      {
        return get<X> (std::string (key));
      }

      template <typename X>
      X&
      get (std::string const& key);

      template <typename X>
      X const&
      get (char const* key) const
      {
        return get<X> (std::string (key));
      }

      template <typename X>
      X const&
      get (std::string const& key) const;

      template <typename X>
      X const&
      get (char const* key, X const& default_value) const
      {
        return get<X> (std::string (key), default_value);
      }

      template <typename X>
      X const&
      get (std::string const& key, X const& default_value) const;

      template <typename X>
      X&
      set (char const* key, X const& value)
      {
        return set<X> (std::string (key), value);
      }

      template <typename X>
      X&
      set (std::string const& key, X const& value);

      void
      set (char const* key, container::any const& value)
      {
        return set (std::string (key), value);
      }

      void
      set (std::string const& key, container::any const& value);

      void
      remove (char const* key)
      {
        remove (std::string (key));
      }

      void
      remove (std::string const& key);

      std::type_info const&
      type_info (char const* key) const
      {
        return type_info (std::string (key));
      }

      std::type_info const&
      type_info (std::string const& key) const;

    private:
      typedef std::map<std::string, container::any> map;

      map map_;
    };
  }
}

#include <libcutl/compiler/context.txx>

#endif // LIBCUTL_COMPILER_CONTEXT_HXX
