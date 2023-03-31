
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

using cpp_numeric = boost::multiprecision::cpp_dec_float_100;

#include "../cmall/include/traits-pgsql.hpp"

int main()
{
    std::string data = "1.5";
    std::string data2 = "1.5";

    cpp_numeric a, b, c = 0;
    a = cpp_numeric(data);
    b = cpp_numeric(data2);

    c += a;
    c += b;


    std::cerr << "c = "  << c << std::endl;
    std::cerr << "a = "  << numeric_to_string(a) << std::endl;
}