// file      : libcutl/compiler/traversal.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_COMPILER_TRAVERSAL_HXX
#define LIBCUTL_COMPILER_TRAVERSAL_HXX

#include <map>
#include <set>
#include <vector>

#include <libcutl/compiler/type-info.hxx>

namespace cutl
{
  namespace compiler
  {
    //
    //
    template<typename B>
    class traverser
    {
    public:
      virtual
      ~traverser ();

      virtual void
      trampoline (B&) = 0;
    };

    //
    //
    template<typename B>
    class traverser_map
    {
    public:
      typedef std::vector<traverser<B>*> traversers;

      struct map_type: std::map<type_id, traversers>
      {
        map_type () {}

        // Don't copy traverser maps. We do it here instead of in
        // traverser_map to pacify GCC's -Wextra insisting we must
        // explicitly initialize virtual bases in copy constructor.
        //
        map_type (map_type const&): std::map<type_id, traversers> () {}
        map_type& operator= (map_type const&) {return *this;}
      };

      typedef typename map_type::const_iterator iterator;

      iterator
      begin () const
      {
        return map_.begin ();
      }

      iterator
      end () const
      {
        return map_.end ();
      }

      void
      add (type_id const& id, traverser<B>& t)
      {
        traversers& travs (map_[id]);
        travs.push_back (&t);
      }

    protected:
      map_type map_;
    };

    //
    //
    template <typename X, typename B>
    class traverser_impl: public traverser<B>,
                          public virtual traverser_map<B>
    {
    public:
      typedef X type;

      traverser_impl ()
      {
        this->add (typeid (type), *this);
      }

      traverser_impl (traverser_impl const&)
      {
        this->add (typeid (type), *this);
      }

      virtual void
      traverse (type&) = 0;

    public:
      virtual void
      trampoline (B&);
    };

    //
    //
    template <typename B>
    class dispatcher: public virtual traverser_map<B>
    {
    public:
      virtual
      ~dispatcher ();

      void
      traverser (traverser_map<B>&);

      virtual void
      dispatch (B&);

    public:
      template <typename I, typename X>
      static void
      iterate_and_dispatch (I begin, I end, dispatcher<X>& d)
      {
        for (; begin != end; ++begin)
        {
          d.dispatch (*begin);
        }
      }

      template <typename T, typename A, typename I, typename X>
      static void
      iterate_and_dispatch (I begin,
                            I end,
                            dispatcher<X>& d,
                            T& t,
                            void (T::*next)(A&),
                            A& a)
      {
        for (; begin != end;)
        {
          d.dispatch (*begin);

          if (++begin != end && next != 0)
            (t.*next) (a);
        }
      }

    private:
      struct comparator
      {
        bool
        operator () (type_info const& a, type_info const& b) const
        {
          return a.type_id () < b.type_id ();
        }
      };

      typedef std::map<type_info, std::size_t, comparator> level_map;
      typedef std::set<type_info, comparator> type_info_set;

      static std::size_t
      compute_levels (type_info const&, std::size_t current, level_map&);

      static void
      flatten_tree (type_info const&, type_info_set&);
    };
  }
}

#include <libcutl/compiler/traversal.txx>

#endif // LIBCUTL_COMPILER_TRAVERSAL_HXX
