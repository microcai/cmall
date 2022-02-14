// file      : libbuild2/cc/utility.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace build2
{
  namespace cc
  {
    inline otype
    compile_type (const target& t, unit_type u)
    {
      using namespace bin;

      auto test = [&t, u] (const auto& h, const auto& i, const auto& o)
      {
        return t.is_a (u == unit_type::module_header ? h :
                       u == unit_type::module_iface  ? i :
                       o);
      };

      return
        test (hbmie::static_type, bmie::static_type, obje::static_type) ? otype::e :
        test (hbmia::static_type, bmia::static_type, obja::static_type) ? otype::a :
        otype::s;
    }

    inline compile_target_types
    compile_types (otype t)
    {
      using namespace bin;

      const target_type* o (nullptr);
      const target_type* i (nullptr);
      const target_type* h (nullptr);

      switch (t)
      {
      case otype::e:
        o = &obje::static_type;
        i = &bmie::static_type;
        h = &hbmie::static_type;
        break;
      case otype::a:
        o = &obja::static_type;
        i = &bmia::static_type;
        h = &hbmia::static_type;
        break;
      case otype::s:
        o = &objs::static_type;
        i = &bmis::static_type;
        h = &hbmis::static_type;
        break;
      }

      return compile_target_types {*o, *i, *h};
    }
  }
}
