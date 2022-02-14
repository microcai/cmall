// file      : libbpkg/manifest.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBPKG_MANIFEST_HXX
#define LIBBPKG_MANIFEST_HXX

#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>    // uint16_t
#include <ostream>
#include <utility>    // move()
#include <stdexcept>  // logic_error
#include <functional>

#include <libbutl/url.mxx>
#include <libbutl/path.mxx>
#include <libbutl/optional.mxx>
#include <libbutl/small-vector.mxx>
#include <libbutl/manifest-forward.hxx>

#include <libbpkg/package-name.hxx>

#include <libbpkg/export.hxx>
#include <libbpkg/version.hxx>

namespace bpkg
{
  using strings = std::vector<std::string>;

  // @@ Let's create <libbpkg/types.hxx> with "basic" package types.
  //
  class LIBBPKG_EXPORT version
  {
  public:
    // Let's keep the members in the order they appear in the string
    // representation. We also make them const to make sure things stay
    // consistent.
    //
    const std::uint16_t epoch;
    const std::string upstream;
    const butl::optional<std::string> release;

    // The absent revision semantics depends on the context the version object
    // is used in. Normally, it is equivalent to zero revision but may have a
    // special meaning, for example, denoting any package revision.
    //
    const butl::optional<std::uint16_t> revision;

    const std::uint32_t iteration;

    // Upstream part canonical representation.
    //
    const std::string canonical_upstream;

    // Release part canonical representation.
    //
    const std::string canonical_release;

    // Create a special empty version. It is less than any other valid
    // version (and is conceptually equivalent to +0-0-).
    //
    version (): epoch (0), release (""), iteration (0) {}

    // By default, treat the zero revision as no revision. Throw
    // std::invalid_argument if the passed string is not a valid version
    // representation.
    //
    explicit
    version (const std::string& v, bool fold_zero_revision = true)
        : version (v.c_str (), fold_zero_revision) {}

    explicit
    version (const char* v, bool fold_zero_revision = true)
        : version (data_type (v, data_type::parse::full, fold_zero_revision))
    {
    }

    // Create the version object from separate epoch, upstream, release,
    // revision, and iteration parts.
    //
    // Note that it is possible (and legal) to create the special empty
    // version via this interface as version(0, string(), string(), nullopt, 0).
    //
    version (std::uint16_t epoch,
             std::string upstream,
             butl::optional<std::string> release,
             butl::optional<std::uint16_t> revision,
             std::uint32_t iteration);

    std::uint16_t
    effective_revision () const noexcept {return revision ? *revision : 0;}

    version (version&&) = default;
    version (const version&) = default;
    version& operator= (version&&);
    version& operator= (const version&);

    // If the revision is ignored, then the iteration (that semantically
    // extends the revision) is also ignored, regardless of the argument.
    //
    std::string
    string (bool ignore_revision = false, bool ignore_iteration = false) const;

    bool
    operator< (const version& v) const noexcept {return compare (v) < 0;}

    bool
    operator> (const version& v) const noexcept {return compare (v) > 0;}

    bool
    operator== (const version& v) const noexcept {return compare (v) == 0;}

    bool
    operator<= (const version& v) const noexcept {return compare (v) <= 0;}

    bool
    operator>= (const version& v) const noexcept {return compare (v) >= 0;}

    bool
    operator!= (const version& v) const noexcept {return compare (v) != 0;}

    // If the revision is ignored, then the iteration is also ignored,
    // regardless of the argument (see above for details).
    //
    int
    compare (const version& v,
             bool ignore_revision = false,
             bool ignore_iteration = false) const noexcept
    {
      if (epoch != v.epoch)
        return epoch < v.epoch ? -1 : 1;

      if (int c = canonical_upstream.compare (v.canonical_upstream))
        return c;

      if (int c = canonical_release.compare (v.canonical_release))
        return c;

      if (!ignore_revision)
      {
        if (revision != v.revision)
          return revision < v.revision ? -1 : 1;

        if (!ignore_iteration && iteration != v.iteration)
          return iteration < v.iteration ? -1 : 1;
      }

      return 0;
    }

    bool
    empty () const noexcept
    {
      bool e (upstream.empty ());

      assert (!e ||
              (epoch == 0 &&
               release && release->empty () &&
               !revision && iteration == 0));

      return e;
    }

  private:
    struct LIBBPKG_EXPORT data_type
    {
      enum class parse {full, upstream, release};

      data_type (const char*, parse, bool fold_zero_revision);

      // Note that there is no iteration component as it can't be present in
      // the string representation passed to the ctor.
      //
      std::uint16_t epoch;
      std::string upstream;
      butl::optional<std::string> release;
      butl::optional<std::uint16_t> revision;
      std::string canonical_upstream;
      std::string canonical_release;
    };

    explicit
    version (data_type&& d)
        : epoch (d.epoch),
          upstream (std::move (d.upstream)),
          release (std::move (d.release)),
          revision (d.revision),
          iteration (0),
          canonical_upstream (std::move (d.canonical_upstream)),
          canonical_release (std::move (d.canonical_release)) {}
  };

  inline std::ostream&
  operator<< (std::ostream& os, const version& v)
  {
    return os << (v.empty () ? "<empty-version>" : v.string ());
  }

  // priority
  //
  class priority
  {
  public:
    enum value_type {low, medium, high, security};

    value_type value; // Shouldn't be necessary to access directly.
    std::string comment;

    priority (value_type v = low, std::string c = "")
        : value (v), comment (std::move (c)) {}

    operator value_type () const {return value;}
  };

  // description
  // description-file
  // change
  // change-file
  //
  class LIBBPKG_EXPORT text_file
  {
  public:
    using path_type = butl::path;

    bool file;

    union
    {
      std::string text;
      path_type path;
    };

    std::string comment;

    // File text constructor.
    //
    explicit
    text_file (std::string t = ""): file (false), text (std::move (t)) {}

    // File reference constructor.
    //
    text_file (path_type p, std::string c)
        : file (true), path (std::move (p)), comment (std::move (c)) {}

    text_file (text_file&&);
    text_file (const text_file&);
    text_file& operator= (text_file&&);
    text_file& operator= (const text_file&);

    ~text_file ();
  };

  // license
  //
  class licenses: public butl::small_vector<std::string, 1>
  {
  public:
    std::string comment;

    explicit
    licenses (std::string c = ""): comment (std::move (c)) {}
  };

  // url
  // doc-url
  // src-url
  // package-url
  //
  // URL that has the following constraints:
  //
  // - is not rootless
  // - is not local (the scheme is not `file`)
  // - authority is present and is not empty
  //
  // See libbutl/url.mxx for details.
  //
  // NOTE: this class must not be DLL-exported wholesale (non-exported base).
  //
  class manifest_url: public butl::url
  {
  public:
    std::string comment;

    // Throw invalid_argument on parsing or constraints checking error.
    //
    explicit LIBBPKG_EXPORT
    manifest_url (const std::string& u, std::string c = "");

    manifest_url () = default;
  };

  // email
  // package-email
  // build-email
  // build-warning-email
  // build-error-email
  //
  class email: public std::string
  {
  public:
    std::string comment;

    explicit
    email (std::string e = "", std::string c = "")
        : std::string (std::move (e)), comment (std::move (c)) {}
  };

  // Represented as a version range. Note that the versions may refer to the
  // dependent package version and can be completed with the actual versions
  // using the effective() function. Such versions are represented as an empty
  // version object and have the dollar character string representation.
  //
  // If the version endpoints are equal and both are closed, then this is the
  // `== <version>` constraint (in particular, `== $` if empty endpoints). If
  // both endpoints are empty and one of them is open, then this is either
  // `~$` (min endpoint is open) or `^$` (max endpoint is open). Note that
  // equal endpoints can never be both open.
  //
  // An absent endpoint version revision has the 'any revision' meaning
  // (except for an earliest release for which the revision is meaningless) and
  // so translates into the effective revision differently, depending on the
  // range endpoint side and openness:
  //
  // [X Y)  ==  [X+0   Y+0)
  // (X Y]  ==  (X+max Y+max]
  //
  class LIBBPKG_EXPORT version_constraint
  {
  public:
    butl::optional<version> min_version;
    butl::optional<version> max_version;
    bool min_open;
    bool max_open;

    // Preserve the zero endpoint version revisions (see above for details).
    //
    explicit
    version_constraint (const std::string&);

    version_constraint (butl::optional<version> min_version, bool min_open,
                        butl::optional<version> max_version, bool max_open);

    explicit
    version_constraint (const version& v)
        : version_constraint (v, false, v, false) {}

    version_constraint () = default;

    bool
    empty () const noexcept {return !min_version && !max_version;}

    bool
    complete () const noexcept
    {
      return (!min_version || !min_version->empty ()) &&
             (!max_version || !max_version->empty ());
    }

    // Return the completed constraint if it refers to the dependent package
    // version and copy of itself otherwise. Throw std::invalid_argument if
    // the resulting constraint is invalid (max version is less than min
    // version in range, non-standard or latest snapshot version for a
    // shortcut operator, etc.).
    //
    version_constraint
    effective (version) const;

    std::string
    string () const;
  };

  inline std::ostream&
  operator<< (std::ostream& os, const version_constraint& vc)
  {
    return os << vc.string ();
  }

  inline bool
  operator== (const version_constraint& x, const version_constraint& y)
  {
    return x.min_version == y.min_version && x.max_version == y.max_version &&
           x.min_open == y.min_open && x.max_open == y.max_open;
  }

  inline bool
  operator!= (const version_constraint& x, const version_constraint& y)
  {
    return !(x == y);
  }

  struct LIBBPKG_EXPORT dependency
  {
    package_name name;
    butl::optional<version_constraint> constraint;

    std::string
    string () const;
  };

  inline std::ostream&
  operator<< (std::ostream& os, const dependency& d)
  {
    return os << d.string ();
  }

  // depends
  //
  class dependency_alternatives: public butl::small_vector<dependency, 1>
  {
  public:
    bool conditional;
    bool buildtime;
    std::string comment;

    dependency_alternatives () = default;
    dependency_alternatives (bool d, bool b, std::string c)
        : conditional (d), buildtime (b), comment (std::move (c)) {}
  };

  LIBBPKG_EXPORT std::ostream&
  operator<< (std::ostream&, const dependency_alternatives&);

  // requires
  //
  class requirement_alternatives: public butl::small_vector<std::string, 1>
  {
  public:
    bool conditional;
    bool buildtime;
    std::string comment;

    requirement_alternatives () = default;
    requirement_alternatives (bool d, bool b, std::string c)
        : conditional (d), buildtime (b), comment (std::move (c)) {}
  };

  class build_constraint
  {
  public:
    // If true, then the package should not be built for matching
    // configurations by automated build bots.
    //
    bool exclusion;

    // Filesystem wildcard patterns for the build configuration name and
    // target.
    //
    std::string config;
    butl::optional<std::string> target;

    std::string comment;

    build_constraint () = default;
    build_constraint (bool e,
                      std::string n,
                      butl::optional<std::string> t,
                      std::string c)
        : exclusion (e),
          config (std::move (n)),
          target (std::move (t)),
          comment (std::move (c)) {}
  };

  // Package manifest value validation forbid/require flags.
  //
  // Some package manifest values can be forbidden or required for certain
  // repository types and in specific contexts (for example, when parsing an
  // individual manifest, a manifest list, etc).
  //
  // Also note that, naturally, the forbid_* and require_* flags are mutually
  // exclusive for the same value.
  //
  enum class package_manifest_flags: std::uint16_t
  {
    none                           = 0x00,

    forbid_file                    = 0x01, // Forbid *-file manifest values.
    forbid_location                = 0x02,
    forbid_sha256sum               = 0x04,
    forbid_fragment                = 0x08,
    forbid_incomplete_dependencies = 0x10,

    require_location               = 0x20,
    require_sha256sum              = 0x40,
    require_description_type       = 0x80
  };

  inline package_manifest_flags
  operator&= (package_manifest_flags& x, package_manifest_flags y)
  {
    return x = static_cast<package_manifest_flags> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline package_manifest_flags
  operator|= (package_manifest_flags& x, package_manifest_flags y)
  {
    return x = static_cast<package_manifest_flags> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }

  inline package_manifest_flags
  operator& (package_manifest_flags x, package_manifest_flags y)
  {
    return x &= y;
  }

  inline package_manifest_flags
  operator| (package_manifest_flags x, package_manifest_flags y)
  {
    return x |= y;
  }

  // Build configuration class term.
  //
  class LIBBPKG_EXPORT build_class_term
  {
  public:
    char operation; // '+', '-' or '&'
    bool inverted;  // Operation is followed by '!'.
    bool simple;    // Name if true, expr otherwise.
    union
    {
      std::string                   name; // Class name.
      std::vector<build_class_term> expr; // Parenthesized expression.
    };

    // Create the simple term object (class name).
    //
    build_class_term (std::string n, char o, bool i)
        : operation (o), inverted (i), simple (true), name (std::move (n)) {}

    // Create the compound term object (parenthesized expression).
    //
    build_class_term (std::vector<build_class_term> e, char o, bool i)
        : operation (o), inverted (i), simple (false), expr (std::move (e)) {}

    // Required by VC for some reason.
    //
    build_class_term ()
        : operation ('\0'), inverted (false), simple (true), name () {}

    build_class_term (build_class_term&&);
    build_class_term (const build_class_term&);
    build_class_term& operator= (build_class_term&&);
    build_class_term& operator= (const build_class_term&);

    ~build_class_term ();

    // Check that the specified string is a valid class name, that is
    // non-empty, containing only alpha-numeric characters, '_', '+', '-', '.'
    // (except as the first character for the last three). Return true if the
    // name is reserved (starts with '_'). Throw std::invalid_argument if
    // invalid.
    //
    static bool
    validate_name (const std::string&);
  };

  // Map of derived build classes to their bases.
  //
  using build_class_inheritance_map = std::map<std::string, std::string>;

  // Build configuration class expression. Includes comment and optional
  // underlying set.
  //
  class LIBBPKG_EXPORT build_class_expr
  {
  public:
    std::string comment;
    strings underlying_classes;
    std::vector<build_class_term> expr;

  public:
    build_class_expr () = default;

    // Parse the string representation of a space-separated build class
    // expression, potentially prepended with a space-separated underlying
    // build class set, in which case the expression can be empty. If both,
    // underlying class set and expression are present, then they should be
    // separated with the semicolon. Throw std::invalid_argument if the
    // representation is invalid. Some expression examples:
    //
    // +gcc
    // -msvc -clang
    // default leagacy
    // default leagacy :
    // default leagacy : -msvc
    // default leagacy : &gcc
    //
    build_class_expr (const std::string&, std::string comment);

    // Create the expression object from a class list (c1, c2, ...) using the
    // specified operation (+/-/&) according to the following rules:
    //
    // +  ->  +c1 +c2 ...
    // -  ->  -c1 -c2 ...
    // &  ->  &( +c1 +c2 ... )
    //
    // An empty class list results in an empty expression.
    //
    // Note: it is assumed that the class names are valid.
    //
    build_class_expr (const strings& classes,
                      char operation,
                      std::string comment);

    // Return the string representation of the build class expression,
    // potentially prepended with the underlying class set.
    //
    std::string
    string () const;

    // Match a build configuration that belongs to the specified list of
    // classes (and recursively to their bases) against the expression. Either
    // return or update the result (the latter allows to sequentially matching
    // against a list of expressions).
    //
    // Notes:
    //
    // - The derived-to-base map is not verified (that there are no
    //   inheritance cycles, etc.).
    //
    // - The underlying class set doesn't affect the match in any way (it
    //   should have been used to pre-filter the set of build configurations).
    //
    void
    match (const strings&,
           const build_class_inheritance_map&,
           bool& result) const;

    bool
    match (const strings& cs, const build_class_inheritance_map& bs) const
    {
      bool r (false);
      match (cs, bs, r);
      return r;
    }
  };

  inline std::ostream&
  operator<< (std::ostream& os, const build_class_expr& bce)
  {
    return os << bce.string ();
  }

  enum class text_type
  {
    plain,
    common_mark,
    github_mark
  };

  LIBBPKG_EXPORT std::string
  to_string (text_type);

  // Throw std::invalid_argument if the argument is not a well-formed text
  // type. Otherwise, return nullopt for an unknown text variant.
  //
  LIBBPKG_EXPORT butl::optional<text_type>
  to_text_type (const std::string&); // May throw std::invalid_argument.

  inline std::ostream&
  operator<< (std::ostream& os, text_type t)
  {
    return os << to_string (t);
  }

  enum class test_dependency_type
  {
    tests,
    examples,
    benchmarks
  };

  LIBBPKG_EXPORT std::string
  to_string (test_dependency_type);

  // May throw std::invalid_argument.
  //
  LIBBPKG_EXPORT test_dependency_type
  to_test_dependency_type (const std::string&);

  inline std::ostream&
  operator<< (std::ostream& os, test_dependency_type t)
  {
    return os << to_string (t);
  }

  struct test_dependency: dependency
  {
    test_dependency_type type;

    test_dependency () = default;
    test_dependency (package_name n,
                     test_dependency_type t,
                     butl::optional<version_constraint> c)
        : dependency {std::move (n), std::move (c)}, type (t) {}
  };

  class LIBBPKG_EXPORT package_manifest
  {
  public:
    using version_type = bpkg::version;
    using priority_type = bpkg::priority;
    using email_type = bpkg::email;

    package_name name;
    version_type version;
    butl::optional<std::string> upstream_version;
    butl::optional<package_name> project;
    butl::optional<priority_type> priority;
    std::string summary;

    // @@ Replace with small_vector<licenses, 1>. Note that currently it is
    //    unsupported by the odb::nested_*() functions that are
    //    std::vector-specific.
    //
    std::vector<licenses> license_alternatives;

    butl::small_vector<std::string, 5> topics;
    butl::small_vector<std::string, 5> keywords;
    butl::optional<text_file> description;
    butl::optional<std::string> description_type;
    butl::small_vector<text_file, 1> changes;
    butl::optional<manifest_url> url;
    butl::optional<manifest_url> doc_url;
    butl::optional<manifest_url> src_url;
    butl::optional<manifest_url> package_url;
    butl::optional<email_type> email;
    butl::optional<email_type> package_email;
    butl::optional<email_type> build_email;
    butl::optional<email_type> build_warning_email;
    butl::optional<email_type> build_error_email;
    std::vector<dependency_alternatives> dependencies;
    std::vector<requirement_alternatives> requirements;
    butl::small_vector<test_dependency, 1> tests;

    butl::small_vector<build_class_expr, 1> builds;
    std::vector<build_constraint> build_constraints;

    // The following values are only valid in the manifest list (and only for
    // certain repository types).
    //
    butl::optional<butl::path> location;
    butl::optional<std::string> sha256sum;
    butl::optional<std::string> fragment;

    const package_name&
    effective_project () const noexcept {return project ? *project : name;}

    // Return the description type value if present, text_type::github_mark if
    // the description refers to a file with the .md or .markdown extension
    // and text_type::plain if it refers to a file with the .txt extension or
    // no extension or the description does not come from a file. Depending on
    // the ignore_unknown value either throw std::invalid_argument or return
    // nullopt if the description value or the file extension is unknown.
    // Throw std::logic_error if the description value is nullopt.
    //
    butl::optional<text_type>
    effective_description_type (bool ignore_unknown = false) const;

  public:
    package_manifest () = default;

    // Create individual manifest.
    //
    // The default package_manifest_flags value corresponds to a valid
    // individual package manifest.
    //
    package_manifest (butl::manifest_parser&,
                      bool ignore_unknown = false,
                      bool complete_dependencies = true,
                      package_manifest_flags =
                        package_manifest_flags::forbid_location  |
                        package_manifest_flags::forbid_sha256sum |
                        package_manifest_flags::forbid_fragment);

    // As above but also call the translate function for the version value
    // passing through any exception it may throw. Throw std::invalid_argument
    // if the resulting version isn't a valid package version (empty, earliest
    // release, etc).
    //
    // In particular, the translation function may "patch" the version with
    // the snapshot information (see <libbutl/standard-version.mxx> for
    // details). This translation is normally required for manifests of
    // packages that are accessed as directories (as opposed to package
    // archives that should have their version already patched).
    //
    using translate_function = void (version_type&);

    package_manifest (butl::manifest_parser&,
                      const std::function<translate_function>&,
                      bool ignore_unknown = false,
                      bool complete_depends = true,
                      package_manifest_flags =
                        package_manifest_flags::forbid_location  |
                        package_manifest_flags::forbid_sha256sum |
                        package_manifest_flags::forbid_fragment);

    // Create an element of the list manifest.
    //
    package_manifest (butl::manifest_parser&,
                      butl::manifest_name_value start,
                      bool ignore_unknown,
                      bool complete_depends,
                      package_manifest_flags);

    // Override manifest values with the specified. Throw manifest_parsing if
    // any value is invalid, cannot be overridden, or its name is not
    // recognized.
    //
    // The specified values override the whole groups they belong to,
    // resetting all the group values prior to being applied. Currently, only
    // the following value groups can be overridden: {build-*email} and
    // {builds, build-{include,exclude}}.
    //
    // Note that the build constraints group values are overridden
    // hierarchically so that the build-{include,exclude} overrides don't
    // affect the builds values.
    //
    // If a non-empty source name is specified, then the specified values are
    // assumed to also include the line/column information and the possibly
    // thrown manifest_parsing exception will contain the invalid value
    // location information. Otherwise, the exception description will refer
    // to the invalid value name instead.
    //
    void
    override (const std::vector<butl::manifest_name_value>&,
              const std::string& source_name);

    // Validate the overrides without applying them to any manifest.
    //
    static void
    validate_overrides (const std::vector<butl::manifest_name_value>&,
                        const std::string& source_name);

    void
    serialize (butl::manifest_serializer&) const;

    // Serialize only package manifest header values.
    //
    void
    serialize_header (butl::manifest_serializer&) const;

    // Load the *-file manifest values using the specified load function that
    // returns the file contents passing through any exception it may throw.
    // Set the potentially absent description type value to the effective
    // description type. If the effective type is nullopt then assign a
    // synthetic unknown type.
    //
    // Note that if the returned file contents is empty, load_files() makes
    // sure that this is allowed by the value's semantics throwing
    // manifest_parsing otherwise. However, the load function may want to
    // recognize such cases itself in order to issue more precise diagnostics.
    //
    using load_function = std::string (const std::string& name,
                                       const butl::path& value);

    void
    load_files (const std::function<load_function>&,
                bool ignore_unknown = false);
  };

  // Create individual package manifest.
  //
  inline package_manifest
  pkg_package_manifest (butl::manifest_parser& p,
                        bool ignore_unknown = false,
                        bool complete_depends = true)
  {
    return package_manifest (p, ignore_unknown, complete_depends);
  }

  LIBBPKG_EXPORT package_manifest
  dir_package_manifest (butl::manifest_parser&, bool ignore_unknown = false);

  LIBBPKG_EXPORT package_manifest
  git_package_manifest (butl::manifest_parser&, bool ignore_unknown = false);

  // Create an element of the package list manifest.
  //
  LIBBPKG_EXPORT package_manifest
  pkg_package_manifest (butl::manifest_parser&,
                        butl::manifest_name_value start,
                        bool ignore_unknown = false);

  LIBBPKG_EXPORT package_manifest
  dir_package_manifest (butl::manifest_parser&,
                        butl::manifest_name_value start,
                        bool ignore_unknown = false);

  LIBBPKG_EXPORT package_manifest
  git_package_manifest (butl::manifest_parser&,
                        butl::manifest_name_value start,
                        bool ignore_unknown = false);

  // Serialize.
  //
  inline void
  pkg_package_manifest (butl::manifest_serializer& s,
                        const package_manifest& m)
  {
    m.serialize (s);
  }

  // Normally there is no need to serialize dir and git package manifests,
  // unless for testing.
  //
  LIBBPKG_EXPORT void
  dir_package_manifest (butl::manifest_serializer&, const package_manifest&);

  LIBBPKG_EXPORT void
  git_package_manifest (butl::manifest_serializer&, const package_manifest&);

  class LIBBPKG_EXPORT pkg_package_manifests:
    public std::vector<package_manifest>
  {
  public:
    using base_type = std::vector<package_manifest>;

    using base_type::base_type;

    // Checksum of the corresponding repository_manifests.
    //
    std::string sha256sum;

  public:
    pkg_package_manifests () = default;
    pkg_package_manifests (butl::manifest_parser&,
                           bool ignore_unknown = false);

    void
    serialize (butl::manifest_serializer&) const;
  };

  class LIBBPKG_EXPORT dir_package_manifests:
    public std::vector<package_manifest>
  {
  public:
    using base_type = std::vector<package_manifest>;

    using base_type::base_type;

  public:
    dir_package_manifests () = default;
    dir_package_manifests (butl::manifest_parser&,
                           bool ignore_unknown = false);

    // Normally there is no need to serialize dir package manifests, unless for
    // testing.
    //
    void
    serialize (butl::manifest_serializer&) const;
  };

  class LIBBPKG_EXPORT git_package_manifests:
    public std::vector<package_manifest>
  {
  public:
    using base_type = std::vector<package_manifest>;

    using base_type::base_type;

  public:
    git_package_manifests () = default;
    git_package_manifests (butl::manifest_parser&,
                           bool ignore_unknown = false);

    // Normally there is no need to serialize git package manifests, unless for
    // testing.
    //
    void
    serialize (butl::manifest_serializer&) const;
  };

  // Traits class for the repository URL object.
  //
  enum class repository_protocol {file, http, https, git, ssh};

  struct LIBBPKG_EXPORT repository_url_traits
  {
    using string_type = std::string;
    using path_type   = butl::path;

    using scheme_type    = repository_protocol;
    using authority_type = butl::basic_url_authority<string_type>;

    static butl::optional<scheme_type>
    translate_scheme (const string_type&,
                      string_type&&,
                      butl::optional<authority_type>&,
                      butl::optional<path_type>&,
                      butl::optional<string_type>&,
                      butl::optional<string_type>&,
                      bool&);

    static string_type
    translate_scheme (string_type&,
                      const scheme_type&,
                      const butl::optional<authority_type>&,
                      const butl::optional<path_type>&,
                      const butl::optional<string_type>&,
                      const butl::optional<string_type>&,
                      bool);

    static path_type
    translate_path (string_type&&);

    static string_type
    translate_path (const path_type&);
  };

  // Repository URL. Note that it represents both the remote (http(s)://,
  // git://, etc) and local (file:// as well as plain directory path)
  // repository URLs. May also be empty.
  //
  // Notes:
  //
  // - For an empty URL object all components are absent. For a non-empty one
  //   the path is always present and normalized. The string representation of
  //   non-empty object with non-empty path never contains the trailing slash
  //   (except for the root path on POSIX system).
  //
  // - For the remote URL object the host name is in the lower case (IPv4/6 are
  //   not supported) and the path is relative.
  //
  // - For the local URL object the path can be relative or absolute. Query
  //   can not be present. Represent the object using the file:// notation if
  //   it is absolute and the authority or fragment is present. Otherwise
  //   represent it as a local path, appending the fragment if present.
  //
  // - repository_url(string) ctor throws std::invalid_argument exception for
  //   an empty string.
  //
  using repository_url = butl::basic_url<repository_protocol,
                                         repository_url_traits>;

  // Repository type.
  //
  enum class repository_type {pkg, dir, git};

  LIBBPKG_EXPORT std::string
  to_string (repository_type);

  LIBBPKG_EXPORT repository_type
  to_repository_type (const std::string&); // May throw std::invalid_argument.

  inline std::ostream&
  operator<< (std::ostream& os, repository_type t)
  {
    return os << to_string (t);
  }

  // Repository basis.
  //
  enum class repository_basis
  {
    archive,
    directory,
    version_control
  };

  // Guess the repository type from the URL:
  //
  // 1. If scheme is git then git.
  //
  // 2. If path has the .git extension then git.
  //
  // 3. If scheme is http(s) or ssh then pkg.
  //
  // 4. If local, check if directory contains the .git/ subdirectory then
  //    git, otherwise pkg.
  //
  // Can throw system_error in the later case.
  //
  LIBBPKG_EXPORT repository_type
  guess_type (const repository_url&, bool local);

  // Repository URL that may have a repository type specified as part of its
  // scheme in the [<type>'+']<protocol> form. For example:
  //
  // git+http://example.com/repo  (repository type + protocol)
  // git://example.com/repo       (protocol only)
  //
  // If the substring preceding the '+' character is not a valid repository
  // type or the part that follows doesn't conform to the repository URL
  // notation, then the whole string is considered to be a repository URL.
  // For example, for all of the following strings the repository URL is
  // untyped (local) and relative:
  //
  // foo+http://example.com/repo  (invalid repository type)
  // git+ftp://example.com/repo   (invalid repository protocol)
  // git+file://example.com/repo  (invalid authority)
  // git+c:/repo                  (not a URL notation)
  //
  // Note also that in quite a few manifests where we specify the location we
  // also allow specifying the type as a separate value. While this may seem
  // redundant (and it now is in a few cases, at least for the time being),
  // keep in mind that for the local relative path the type cannot be
  // specified as part of the URL (since its representation is a non-URL).
  //
  struct LIBBPKG_EXPORT typed_repository_url
  {
    repository_url url;
    butl::optional<repository_type> type;

    explicit
    typed_repository_url (const std::string&);
  };

  class LIBBPKG_EXPORT repository_location
  {
  public:
    // Create a special empty repository_location.
    //
    repository_location () = default;

    // Create a remote or absolute repository location from a potentially
    // typed repository URL (see above).
    //
    // If the type is not specified in the URL scheme then use the one passed
    // as an argument or, if not present, guess it according to the specified
    // local flag (see above). Throw std::invalid_argument if the argument
    // doesn't represent a valid remote or absolute repository location or
    // mismatching types are specified in the URL scheme and in the argument.
    // Underlying OS errors (which may happen when guessing the type when the
    // local flag is set) are reported by throwing std::system_error.
    //
    explicit
    repository_location (
      const std::string&,
      const butl::optional<repository_type>& = butl::nullopt,
      bool local = false);

    // Create remote, absolute or empty repository location making sure that
    // the URL matches the repository type. Throw std::invalid_argument if the
    // URL object is a relative local path.
    //
    // Note that the repository location string representation may differ from
    // the original URL in the presence of the trailing slash. This may cause
    // problems with some WEB servers that are sensitive to the trailing slash
    // presence/absence. For example:
    //
    // $ git clone http://git.sv.gnu.org/r/config.git
    //   warning: redirecting to http://git.savannah.gnu.org/r/config.git/
    //
    // Also note that we disregard the slash presence/absence on multiple
    // levels:
    //
    // - reduce absent path to an empty one in
    //   repository_url_traits::translate_scheme() (so a.com/ becomes a.com)
    //
    // - use path::*string() rather than path::*representation() functions
    //   in repository_url_traits::translate_*() functions
    //
    // - may append slash in repository_location ctor
    //
    repository_location (repository_url, repository_type);

    // Create a potentially relative repository location. If base is not
    // empty, use it to complete the relative location to the remote/absolute
    // one. Throw std::invalid_argument if base is not empty but the location
    // is empty, base itself is relative, or the resulting completed location
    // is invalid.
    //
    repository_location (repository_url,
                         repository_type,
                         const repository_location& base);

    repository_location (const repository_location& l,
                         const repository_location& base)
        : repository_location (l.url (), l.type (), base) {}

    // Note that relative locations have no canonical name. Canonical name of
    // an empty location is the empty name.
    //
    const std::string&
    canonical_name () const noexcept {return canonical_name_;}

    // There are 3 types of locations: remote, local absolute filesystem
    // path and local relative filesystem path. Plus there is the special
    // empty location. The following predicates can be used to determine
    // what kind of location it is. Note that except for empty(), all the
    // other predicates throw std::logic_error for an empty location.
    //
    bool
    empty () const noexcept {return url_.empty ();}

    bool
    local () const
    {
      if (empty ())
        throw std::logic_error ("empty location");

      return url_.scheme == repository_protocol::file;
    }

    bool
    remote () const
    {
      return !local ();
    }

    bool
    absolute () const
    {
      if (empty ())
        throw std::logic_error ("empty location");

      // Note that in remote locations path is always relative.
      //
      return url_.path->absolute ();
    }

    bool
    relative () const
    {
      return local () && url_.path->relative ();
    }

    repository_type
    type () const
    {
      if (empty ())
        throw std::logic_error ("empty location");

      return type_;
    }

    repository_basis
    basis () const
    {
      switch (type ())
      {
      case repository_type::pkg: return repository_basis::archive;
      case repository_type::dir: return repository_basis::directory;
      case repository_type::git: return repository_basis::version_control;
      }

      assert (false); // Can't be here.
      return repository_basis::archive;
    }

    // Note that the URL of an empty location is empty.
    //
    const repository_url&
    url () const
    {
      return url_;
    }

    // Repository path. Note that for repository types that refer to
    // "directories" it always contains the trailing slash.
    //
    const butl::path&
    path () const
    {
      if (empty ())
        throw std::logic_error ("empty location");

      return *url_.path;
    }

    const std::string&
    host () const
    {
      if (local ())
        throw std::logic_error ("local location");

      return url_.authority->host;
    }

    // Value 0 indicated that no port was specified explicitly.
    //
    std::uint16_t
    port () const
    {
      if (local ())
        throw std::logic_error ("local location");

      return url_.authority->port;
    }

    repository_protocol
    proto () const
    {
      if (empty ())
        throw std::logic_error ("empty location");

      return url_.scheme;
    }

    const butl::optional<std::string>&
    fragment () const
    {
      if (relative ())
        throw std::logic_error ("relative filesystem path");

      return url_.fragment;
    }

    bool
    archive_based () const
    {
      return basis () == repository_basis::archive;
    }

    bool
    directory_based () const
    {
      return basis () == repository_basis::directory;
    }

    bool
    version_control_based () const
    {
      return basis () == repository_basis::version_control;
    }

    // Return an untyped URL if the correct type can be guessed just from
    // the URL. Otherwise, return the typed URL.
    //
    // String representation is empty for an empty location and is always
    // untyped for the relative location (which is a non-URL).
    //
    std::string
    string () const;

  private:
    std::string canonical_name_;
    repository_url url_;
    repository_type type_;
  };

  inline std::ostream&
  operator<< (std::ostream& os, const repository_location& l)
  {
    return os << l.string ();
  }

  // Git refname/pattern and/or commit. If none of them is present then the
  // default reference set is assumed. If both are present then the commit is
  // expected to belong to the history of the specified refs (e.g., tag or
  // branch). Note that the name member can also be an abbreviated commit id
  // (full, 40-character commit ids should always be stored in the commit
  // member since they may refer to an unadvertised commit).
  //
  class LIBBPKG_EXPORT git_ref_filter
  {
  public:
    butl::optional<std::string> name;
    butl::optional<std::string> commit;
    bool exclusion = false;

  public:
    git_ref_filter () = default; // Default reference set.

    // Parse the [+|-][<name>][@<commit>] reference filter representation.
    // Throw std::invalid_argument if the string is empty or the filter
    // representation format is invalid.
    //
    explicit
    git_ref_filter (const std::string&);

    git_ref_filter (butl::optional<std::string> n,
                    butl::optional<std::string> c,
                    bool e)
        : name (std::move (n)),
          commit (std::move (c)),
          exclusion (e) {}

    bool
    default_refs () const {return !name && !commit;}
  };

  using git_ref_filters = butl::small_vector<git_ref_filter, 2>;

  // Parse a comma-separated list of git reference filters. If the argument
  // starts with the '#' character then prepend the resulting list with the
  // default reference set filter (see above). If the argument is absent then
  // return the list containing a single default reference set filter. Throw
  // std::invalid_argument if the filter list format is invalid.
  //
  LIBBPKG_EXPORT git_ref_filters
  parse_git_ref_filters (const butl::optional<std::string>&);

  enum class repository_role
  {
    base,
    prerequisite,
    complement
  };

  class LIBBPKG_EXPORT repository_manifest
  {
  public:
    using email_type = bpkg::email;

    repository_location location;         // Non-empy for non-base roles.
    butl::optional<repository_role> role;

    // The following values may only be present for the base repository (and
    // only for certain repository types).
    //
    butl::optional<std::string> url;
    butl::optional<email_type>  email;
    butl::optional<std::string> summary;
    butl::optional<std::string> description;
    butl::optional<std::string> certificate;

    // The repository fingerprint to trust. May only be present for the
    // prerequisite or complement repository and only for repository types
    // that support authentication (currently only pkg).
    //
    butl::optional<std::string> trust;

    // The repository fragment id this repository belongs to (may only be
    // present for multi-fragment repositories).
    //
    butl::optional<std::string> fragment;

    // Return the effective role of the repository. If the role is not
    // explicitly specified, then the base role is assumed.
    //
    repository_role
    effective_role () const noexcept
    {
      return role ? *role : repository_role::base;
    }

    // Return the effective web interface URL based on the specified remote
    // repository location. If url is not present, doesn't start with '.', or
    // the repository type differs from pkg, then return it unchanged.
    // Otherwise, process the relative format as described in the manifest
    // specification. Throw std::invalid_argument if the relative url format is
    // invalid or if the repository location is empty or local.
    //
    butl::optional<std::string>
    effective_url (const repository_location&) const;

  public:
    repository_manifest () = default; // VC export.

    void
    serialize (butl::manifest_serializer&) const;
  };

  // Create individual repository manifest.
  //
  LIBBPKG_EXPORT repository_manifest
  pkg_repository_manifest (butl::manifest_parser&,
                           bool ignore_unknown = false);

  LIBBPKG_EXPORT repository_manifest
  dir_repository_manifest (butl::manifest_parser&,
                           bool ignore_unknown = false);

  LIBBPKG_EXPORT repository_manifest
  git_repository_manifest (butl::manifest_parser&,
                           bool ignore_unknown = false);

  // Create an element of the repository list manifest.
  //
  LIBBPKG_EXPORT repository_manifest
  pkg_repository_manifest (butl::manifest_parser&,
                           butl::manifest_name_value start,
                           bool ignore_unknown = false);

  LIBBPKG_EXPORT repository_manifest
  dir_repository_manifest (butl::manifest_parser&,
                           butl::manifest_name_value start,
                           bool ignore_unknown = false);

  LIBBPKG_EXPORT repository_manifest
  git_repository_manifest (butl::manifest_parser&,
                           butl::manifest_name_value start,
                           bool ignore_unknown = false);

  class LIBBPKG_EXPORT pkg_repository_manifests:
    public std::vector<repository_manifest>
  {
  public:
    using base_type = std::vector<repository_manifest>;

    using base_type::base_type;

    pkg_repository_manifests () = default;
    pkg_repository_manifests (butl::manifest_parser&,
                              bool ignore_unknown = false);

    void
    serialize (butl::manifest_serializer&) const;
  };

  class LIBBPKG_EXPORT dir_repository_manifests:
    public std::vector<repository_manifest>
  {
  public:
    using base_type = std::vector<repository_manifest>;

    using base_type::base_type;

    dir_repository_manifests () = default;
    dir_repository_manifests (butl::manifest_parser&,
                              bool ignore_unknown = false);

    void
    serialize (butl::manifest_serializer&) const;
  };

  class LIBBPKG_EXPORT git_repository_manifests:
    public std::vector<repository_manifest>
  {
  public:
    using base_type = std::vector<repository_manifest>;

    using base_type::base_type;

    git_repository_manifests () = default;
    git_repository_manifests (butl::manifest_parser&,
                              bool ignore_unknown = false);

    void
    serialize (butl::manifest_serializer&) const;
  };

  // Search a repository manifest list for the base repository and return its
  // reference, if found. Otherwise, return a reference to an empty manifest
  // instance (which is the representation of the default base).
  //
  LIBBPKG_EXPORT const repository_manifest&
  find_base_repository (const std::vector<repository_manifest>&) noexcept;

  class LIBBPKG_EXPORT signature_manifest
  {
  public:
    // Checksum of the corresponding package_manifests.
    //
    std::string sha256sum;

    // Signature of the corresponding package_manifests. Calculated by
    // encrypting package_manifests checksum (stored in sha256sum) with the
    // repository certificate private key.
    //
    std::vector<char> signature;

  public:
    signature_manifest () = default;
    signature_manifest (butl::manifest_parser&, bool ignore_unknown = false);

    // Serialize sha256sum and base64-encoded representation of the signature.
    //
    void
    serialize (butl::manifest_serializer&) const;

  private:
    // Used for delegating in public constructor. Strictly speaking is not
    // required, as a signature_manifest currently never appears as a part of
    // a manifest list, but kept for the consistency with other manifests
    // implementations.
    //
    signature_manifest (butl::manifest_parser&,
                        butl::manifest_name_value start,
                        bool ignore_unknown);
  };
}

#endif // LIBBPKG_MANIFEST_HXX
