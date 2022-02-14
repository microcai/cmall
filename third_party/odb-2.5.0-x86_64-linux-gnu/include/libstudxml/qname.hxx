// file      : libstudxml/qname.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBSTUDXML_QNAME_HXX
#define LIBSTUDXML_QNAME_HXX

#include <libstudxml/details/pre.hxx>

#include <string>
#include <iosfwd>

#include <libstudxml/forward.hxx>

#include <libstudxml/details/export.hxx>

namespace xml
{
  // Note that the optional prefix is just a "syntactic sugar". In
  // particular, it is ignored by the comparison operators and the
  // std::ostream insertion operator.
  //
  class LIBSTUDXML_EXPORT qname
  {
  public:
    qname () {}
    qname (const std::string& name): name_ (name) {}
    qname (const std::string& ns, const std::string& name)
      : ns_ (ns), name_ (name) {}
    qname (const std::string& ns,
           const std::string& name,
           const std::string& prefix)
      : ns_ (ns), name_ (name), prefix_ (prefix) {}

    const std::string& namespace_ () const {return ns_;}
    const std::string& name () const {return name_;}
    const std::string& prefix () const {return prefix_;}

    std::string& namespace_ () {return ns_;}
    std::string& name () {return name_;}
    std::string& prefix () {return prefix_;}

    bool
    empty () const {return name_.empty () && ns_.empty ();}

    // String representation in the [<namespace>#]<name> form.
    //
    std::string
    string () const;

    // Note that comparison operators ignore prefixes.
    //
  public:
    friend bool
    operator< (const qname& x, const qname& y)
    {
      return x.ns_ < y.ns_ || (x.ns_ == y.ns_ && x.name_ < y.name_);
    }

    friend bool
    operator== (const qname& x, const qname& y)
    {
      return x.ns_ == y.ns_ && x.name_ == y.name_;
    }

    friend bool
    operator!= (const qname& x, const qname& y)
    {
      return !(x == y);
    }

  private:
    std::string ns_;
    std::string name_;
    std::string prefix_;
  };

  // Print the string representation ([<namespace>#]<name>).
  //
  LIBSTUDXML_EXPORT std::ostream&
  operator<< (std::ostream&, const qname&);
}

#include <libstudxml/details/post.hxx>

#endif // LIBSTUDXML_QNAME_HXX
