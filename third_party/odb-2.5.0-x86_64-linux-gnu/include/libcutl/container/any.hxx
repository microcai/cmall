// file      : libcutl/container/any.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_CONTAINER_ANY_HXX
#define LIBCUTL_CONTAINER_ANY_HXX

#include <memory>   // std::unique_ptr/auto_ptr
#include <typeinfo> // std::type_info

#include <libcutl/exception.hxx>

namespace cutl
{
  namespace container
  {
    class any
    {
    public:
      struct typing: exception {};

    public:
      any ()
      {
      }

      template <typename X>
      any (X const& x)
          : holder_ (new holder_impl<X> (x))
      {
      }

      any (any const& x)
          : holder_ (x.holder_->clone ())
      {
      }

      template <typename X>
      any&
      operator= (X const& x)
      {
        holder_.reset (new holder_impl<X> (x));
        return *this;
      }

      any&
      operator= (any const& x)
      {
        holder_.reset (x.holder_->clone ());
        return *this;
      }

    public:
      template <typename X>
      X&
      value ()
      {
        if (holder_impl<X>* p = dynamic_cast<holder_impl<X>*> (holder_.get ()))
          return p->value ();
        else
          throw typing ();
      }

      template <typename X>
      X const&
      value () const
      {
        if (holder_impl<X>* p = dynamic_cast<holder_impl<X>*> (holder_.get ()))
          return p->value ();
        else
          throw typing ();
      }

      std::type_info const&
      type_info () const
      {
        return holder_->type_info ();
      }

    public:
      bool
      empty () const
      {
        return holder_.get () == 0;
      }

      void
      reset ()
      {
        return holder_.reset ();
      }

    private:
      class LIBCUTL_EXPORT holder
      {
      public:
        virtual
        ~holder () {}

        virtual holder*
        clone () const = 0;

        virtual std::type_info const&
        type_info () const = 0;
      };

      template <typename X>
      class holder_impl: public holder
      {
      public:
        holder_impl (X const& x)
            : x_ (x)
        {
        }

        virtual holder_impl*
        clone () const
        {
          return new holder_impl (x_);
        }

        virtual std::type_info const&
        type_info () const
        {
          return typeid (x_);
        }

        X const&
        value () const
        {
          return x_;
        }

        X&
        value ()
        {
          return x_;
        }

      private:
        X x_;
      };

    private:
      std::unique_ptr<holder> holder_;
    };
  }
}

#endif // LIBCUTL_CONTAINER_ANY_HXX
