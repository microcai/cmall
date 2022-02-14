// file      : libbuild2/config/operation.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUILD2_CONFIG_OPERATION_HXX
#define LIBBUILD2_CONFIG_OPERATION_HXX

#include <set>

#include <libbuild2/types.hxx>
#include <libbuild2/utility.hxx>

#include <libbuild2/operation.hxx>

namespace build2
{
  namespace config
  {
    extern const meta_operation_info mo_configure;
    extern const meta_operation_info mo_disfigure;

    const string&
    preprocess_create (context&,
                       values&,
                       vector_view<opspec>&,
                       bool,
                       const location&);

    // Configuration exporting.
    //
    using project_set = std::set<const scope*>; // Pointers for comparison.

    // If inherit is false, then don't rely on inheritance from outer scopes
    // (used for config.config.save/$config.save()). In this case the already
    // configured project set can be empty.
    //
    void
    save_config (const scope& rs,
                 ostream&, const path_name&,
                 bool inherit,
                 const project_set&);
  }
}

#endif // LIBBUILD2_CONFIG_OPERATION_HXX
