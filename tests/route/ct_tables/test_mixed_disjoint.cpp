// The mixed disjoint tables at m = 10, 100 and 1,000 (see ct_table_case.hpp).
#include "mixed_disjoint_10.hpp"
#include "mixed_disjoint_100.hpp"
#include "mixed_disjoint_1000.hpp"

TEST(RouteCtTables, MixedDisjoint10)
{
	ct_tables::check<ct_tables::mixed_disjoint_10::routes, ct_tables::mixed_disjoint_10::probes>();
}

TEST(RouteCtTables, MixedDisjoint100)
{
	ct_tables::check<ct_tables::mixed_disjoint_100::routes, ct_tables::mixed_disjoint_100::probes>();
}

TEST(RouteCtTables, MixedDisjoint1000)
{
	ct_tables::check<ct_tables::mixed_disjoint_1000::routes, ct_tables::mixed_disjoint_1000::probes>();
}
