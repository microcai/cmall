// file      : libcutl/shared-ptr/base.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef LIBCUTL_SHARED_PTR_BASE_HXX
#define LIBCUTL_SHARED_PTR_BASE_HXX

#include <new>
#include <cstddef>   // std::size_t

#include <libcutl/exception.hxx>

#include <libcutl/export.hxx>

namespace cutl
{
  struct share
  {
    explicit
    share (char id);

    bool
    operator== (share) const;

  private:
    char id_;
  };
}

extern LIBCUTL_EXPORT cutl::share shared;
extern LIBCUTL_EXPORT cutl::share exclusive;

LIBCUTL_EXPORT void*
operator new (std::size_t, cutl::share);

LIBCUTL_EXPORT void
operator delete (void*, cutl::share) noexcept;

namespace cutl
{
  struct LIBCUTL_EXPORT not_shared: exception
  {
    virtual char const*
    what () const noexcept;
  };

  struct LIBCUTL_EXPORT shared_base
  {
    shared_base ();
    shared_base (shared_base const&);
    shared_base&
    operator= (shared_base const&);

    void
    _inc_ref ();

    bool
    _dec_ref ();

    std::size_t
    _ref_count () const;

    void*
    operator new (std::size_t, share);

    void
    operator delete (void*, share) noexcept;

    void
    operator delete (void*) noexcept;

  protected:
    std::size_t counter_;
  };

  template <typename X>
  inline X*
  inc_ref (X*);

  template <typename X>
  inline void
  dec_ref (X*);

  template <typename X>
  inline std::size_t
  ref_count (X const*);
}

#include <libcutl/shared-ptr/base.ixx>
#include <libcutl/shared-ptr/base.txx>

#endif // LIBCUTL_SHARED_PTR_BASE_HXX
