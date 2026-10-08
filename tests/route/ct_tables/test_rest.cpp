// The rest tables at m = 10, 100 and 1,000 (see ct_table_case.hpp).
#include "rest_10.hpp"
#include "rest_100.hpp"
#include "rest_1000.hpp"

TEST(RouteCtTables, Rest10)
{
	ct_tables::check<ct_tables::rest_10::routes, ct_tables::rest_10::probes>();
}

TEST(RouteCtTables, Rest100)
{
	ct_tables::check<ct_tables::rest_100::routes, ct_tables::rest_100::probes>();
}

TEST(RouteCtTables, Rest1000)
{
	ct_tables::check<ct_tables::rest_1000::routes, ct_tables::rest_1000::probes>();
}
