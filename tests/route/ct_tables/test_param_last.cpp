// The param last tables at m = 10, 100 and 1,000 (see ct_table_case.hpp).
#include "param_last_10.hpp"
#include "param_last_100.hpp"
#include "param_last_1000.hpp"

TEST(RouteCtTables, ParamLast10)
{
	ct_tables::check<ct_tables::param_last_10::routes, ct_tables::param_last_10::probes>();
}

TEST(RouteCtTables, ParamLast100)
{
	ct_tables::check<ct_tables::param_last_100::routes, ct_tables::param_last_100::probes>();
}

TEST(RouteCtTables, ParamLast1000)
{
	ct_tables::check<ct_tables::param_last_1000::routes, ct_tables::param_last_1000::probes>();
}
