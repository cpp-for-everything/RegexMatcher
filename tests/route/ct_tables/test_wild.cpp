// The wild tables at m = 10, 100 and 1,000 (see ct_table_case.hpp).
#include "wild_10.hpp"
#include "wild_100.hpp"
#include "wild_1000.hpp"

TEST(RouteCtTables, Wild10)
{
	ct_tables::check<ct_tables::wild_10::routes, ct_tables::wild_10::probes>();
}

TEST(RouteCtTables, Wild100)
{
	ct_tables::check<ct_tables::wild_100::routes, ct_tables::wild_100::probes>();
}

TEST(RouteCtTables, Wild1000)
{
	ct_tables::check<ct_tables::wild_1000::routes, ct_tables::wild_1000::probes>();
}
