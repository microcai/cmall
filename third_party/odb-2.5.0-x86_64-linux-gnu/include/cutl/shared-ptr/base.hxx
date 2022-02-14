// file      : cutl/shared-ptr/base.hxx
// license   : MIT; see accompanying LICENSE file

#ifndef CUTL_SHARED_PTR_BASE_HXX
#define CUTL_SHARED_PTR_BASE_HXX

#include <new>
#include <cstddef>   // std::size_t

#include <cutl/exception.hxx>

#include <cutl/details/config.hxx>
#include <cutl/details/export.hxx>

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

#ifdef LIBCUTL_CXX11
LIBCUTL_EXPORT void*
operator new (std::size_t, cutl::share);
#else
LIBCUTL_EXPORT void*
operator new (std::size_t, cutl::share) throw (std::bad_alloc);
#endif

LIBCUTL_EXPORT void
operator delete (void*, cutl::share) LIBCUTL_NOTHROW_NOEXCEPT;

namespace cutl
{
  struct LIBCUTL_EXPORT not_shared: exception
  {
    virtual char const*
    what () const LIBCUTL_NOTHROW_NOEXCEPT;
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

#ifdef LIBCUTL_CXX11
    void*
    operator new (std::size_t, share);
#else
    void*
    operator new (std::size_t, share) throw (std::bad_alloc);
#endif

    void
    operator delete (void*, share) LIBCUTL_NOTHROW_NOEXCEPT;

    void
    operator delete (void*) LIBCUTL_NOTHROW_NOEXCEPT;

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

#include <cutl/shared-ptr/base.ixx>
#include <cutl/shared-ptr/base.txx>

#endif // CUTL_SHARED_PTR_BASE_HXX
