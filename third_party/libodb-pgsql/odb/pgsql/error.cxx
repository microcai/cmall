// file      : odb/pgsql/error.cxx
// license   : GNU GPL v2; see accompanying LICENSE file

#include <string>
#include <cassert>

#include <odb/pgsql/error.hxx>
#include <odb/pgsql/exceptions.hxx>
#include <odb/pgsql/connection.hxx>

using namespace std;

namespace odb
{
  namespace pgsql
  {
    void
    translate_error (connection& c, PGresult* r,
                     size_t pos, multiple_exceptions* mex)
    {
      if (!r)
      {
        if (PQstatus (c.handle ()) == CONNECTION_BAD)
        {
          c.mark_failed ();
          throw connection_lost ();
        }
        else
          throw bad_alloc ();
      }

      // Note that we expect the caller to handle PGRES_PIPELINE_ABORTED since
      // it's not really an error but rather an indication that no attempt was
      // made to execute this statement.
      //
      string ss;
      switch (PQresultStatus (r))
      {
      case PGRES_BAD_RESPONSE:
        {
          throw database_exception ("bad server response");
        }
      case PGRES_FATAL_ERROR:
        {
          const char* s (PQresultErrorField (r, PG_DIAG_SQLSTATE));
          ss = (s != 0 ? s : "?????");

          // Deadlock detected.
          //
          if (ss == "40001" || ss == "40P01")
            throw deadlock ();
          else if (PQstatus (c.handle ()) == CONNECTION_BAD)
          {
            c.mark_failed ();
            throw connection_lost ();
          }
          break;
        }
      default:
        assert (false);
        break;
      }

      string msg;
      {
        // Can be NULL in case of PGRES_BAD_RESPONSE.
        //
        const char* m (PQresultErrorMessage (r));
        msg = (m != 0 ? m : "bad server response");

        // Get rid of the trailing newline if there is one.
        //
        string::size_type n (msg.size ());
        if (n != 0 && msg[n - 1] == '\n')
          msg.resize (n - 1);
      }

      if (mex == 0)
        throw database_exception (ss, msg);
      else
        // In PosgreSQL all errors are fatal.
        //
        mex->insert (pos, database_exception (ss, msg), true);
    }
  }
}
