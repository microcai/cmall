#include <iostream>
#include "libpq-fe.h"

using namespace std;

int main(void)
{
	PGconn *conn;
	const char *info = "hostaddr = '127.0.0.1' port = '4321' dbname = 'avim' user = 'postgres' password = 'xyz' connect_timeout = '3'";
	conn = PQconnectdb(info);
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "Connection to database failed: %s",
			PQerrorMessage(conn));
		return -1;
	}
	return 0;
}
