// file      : odb/boost/date-time/oracle/gregorian-mapping.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_BOOST_DATE_TIME_ORACLE_GREGORIAN_MAPPING_HXX
#define ODB_BOOST_DATE_TIME_ORACLE_GREGORIAN_MAPPING_HXX

#include <boost/date_time/gregorian/gregorian_types.hpp>

// By default map boost::gregorian::date to Oracle DATE. We use the
// NULL value to represent not_a_date_time.
//
#pragma db value(boost::gregorian::date) type("DATE") null

#endif // ODB_BOOST_DATE_TIME_ORACLE_GREGORIAN_MAPPING_HXX
