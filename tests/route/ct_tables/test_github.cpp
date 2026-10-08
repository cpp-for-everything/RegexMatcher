// The 203 routes of the GitHub API list of go-http-routing-benchmark (see ct_table_case.hpp).
#include "github_203.hpp"

TEST(RouteCtTables, Github203)
{
	ct_tables::check<ct_tables::github_203::routes, ct_tables::github_203::probes>();
}
