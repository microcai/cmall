// file      : libbuild2/cxx/target.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUILD2_CXX_TARGET_HXX
#define LIBBUILD2_CXX_TARGET_HXX

#include <libbuild2/types.hxx>
#include <libbuild2/utility.hxx>

#include <libbuild2/target.hxx>
#include <libbuild2/cc/target.hxx>

#include <libbuild2/cxx/export.hxx>

namespace build2
{
  namespace cxx
  {
    using cc::h;
    using cc::c;

    class LIBBUILD2_CXX_SYMEXPORT hxx: public cc::cc
    {
    public:
      using cc::cc;

    public:
      static const target_type static_type;
      virtual const target_type& dynamic_type () const {return static_type;}
    };

    class LIBBUILD2_CXX_SYMEXPORT ixx: public cc::cc
    {
    public:
      using cc::cc;

    public:
      static const target_type static_type;
      virtual const target_type& dynamic_type () const {return static_type;}
    };

    class LIBBUILD2_CXX_SYMEXPORT txx: public cc::cc
    {
    public:
      using cc::cc;

    public:
      static const target_type static_type;
      virtual const target_type& dynamic_type () const {return static_type;}
    };

    class LIBBUILD2_CXX_SYMEXPORT cxx: public cc::cc
    {
    public:
      using cc::cc;

    public:
      static const target_type static_type;
      virtual const target_type& dynamic_type () const {return static_type;}
    };

    // The module interface unit is both like a header (e.g., we need to
    // install it) and like a source (we need to compile it). Plus, to
    // support dual use (modules/headers) it could actually be #include'd
    // (and even in both cases e.g., by different codebases).
    //
    class LIBBUILD2_CXX_SYMEXPORT mxx: public cc::cc
    {
    public:
      using cc::cc;

    public:
      static const target_type static_type;
      virtual const target_type& dynamic_type () const {return static_type;}
    };
  }
}

#endif // LIBBUILD2_CXX_TARGET_HXX
