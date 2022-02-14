// file      : libbuild2/in/rule.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUILD2_IN_RULE_HXX
#define LIBBUILD2_IN_RULE_HXX

#include <libbuild2/types.hxx>
#include <libbuild2/utility.hxx>

#include <libbuild2/rule.hxx>

#include <libbuild2/in/export.hxx>

namespace build2
{
  namespace in
  {
    // Preprocess an .in file.
    //
    // Note that a derived rule can use the target data pad to cache data
    // (e.g., in match()) to be used in substitute/lookup() calls.
    //
    // Note also that currently this rule ignores the dry-run mode (see
    // perform_update() for the rationale).
    //
    class LIBBUILD2_IN_SYMEXPORT rule: public simple_rule
    {
    public:
      // The rule id is used to form the rule name/version entry in depdb. The
      // program argument is the pseudo-program name to use in the command
      // line diagnostics.
      //
      rule (string rule_id,
            string program,
            char symbol = '$',
            bool strict = true)
          : rule_id_ (move (rule_id)),
            program_ (move (program)),
            symbol_ (symbol),
            strict_ (strict) {}

      virtual bool
      match (action, target&, const string&) const override;

      virtual recipe
      apply (action, target&) const override;

      virtual target_state
      perform_update (action, const target&) const;

      // Customization hooks.
      //

      // Perform prerequisite search.
      //
      virtual prerequisite_target
      search (action,
              const target&,
              const prerequisite_member&,
              include_type) const;

      // Perform variable lookup.
      //
      virtual string
      lookup (const location&,
              action,
              const target&,
              const string& name) const;

      // Perform variable substitution. Return nullopt if it should be
      // ignored.
      //
      virtual optional<string>
      substitute (const location&,
                  action,
                  const target&,
                  const string& name,
                  bool strict) const;

    protected:
      const string rule_id_;
      const string program_;
      char symbol_;
      bool strict_;
    };
  }
}

#endif // LIBBUILD2_IN_RULE_HXX
