// The static tables at m = 10, 100 and 1,000 (see ct_table_case.hpp).
#include "static_10.hpp"
#include "static_100.hpp"
#include "static_1000.hpp"

TEST(RouteCtTables, Static10)
{
	ct_tables::check<ct_tables::static_10::routes, ct_tables::static_10::probes>();
}

TEST(RouteCtTables, Static100)
{
	ct_tables::check<ct_tables::static_100::routes, ct_tables::static_100::probes>();
}

TEST(RouteCtTables, Static1000)
{
	ct_tables::check<ct_tables::static_1000::routes, ct_tables::static_1000::probes>();
}
