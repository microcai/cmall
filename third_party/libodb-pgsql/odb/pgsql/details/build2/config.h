/* file      : odb/pgsql/details/build2/config.h
 * license   : GNU GPL v2; see accompanying LICENSE file
 */

/* Static configuration file for the build2 build. The installed case
   (when LIBODB_PGSQL_BUILD2 is not necessarily defined) is the only
   reason we have it. */

#ifndef ODB_PGSQL_DETAILS_CONFIG_H
#define ODB_PGSQL_DETAILS_CONFIG_H

/* Define LIBODB_PGSQL_BUILD2 for the installed case. */
#ifndef LIBODB_PGSQL_BUILD2
#  define LIBODB_PGSQL_BUILD2
#endif

#endif /* ODB_PGSQL_DETAILS_CONFIG_H */
