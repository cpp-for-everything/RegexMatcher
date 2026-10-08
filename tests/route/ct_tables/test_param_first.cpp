// The param first tables at m = 10, 100 and 1,000 (see ct_table_case.hpp).
#include "param_first_10.hpp"
#include "param_first_100.hpp"
#include "param_first_1000.hpp"

TEST(RouteCtTables, ParamFirst10)
{
	ct_tables::check<ct_tables::param_first_10::routes, ct_tables::param_first_10::probes>();
}

TEST(RouteCtTables, ParamFirst100)
{
	ct_tables::check<ct_tables::param_first_100::routes, ct_tables::param_first_100::probes>();
}

TEST(RouteCtTables, ParamFirst1000)
{
	ct_tables::check<ct_tables::param_first_1000::routes, ct_tables::param_first_1000::probes>();
}
