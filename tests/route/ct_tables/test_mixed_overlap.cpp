// The mixed overlap tables at m = 10, 100 and 1,000 (see ct_table_case.hpp).
#include "mixed_overlap_10.hpp"
#include "mixed_overlap_100.hpp"
#include "mixed_overlap_1000.hpp"

TEST(RouteCtTables, MixedOverlap10)
{
	ct_tables::check<ct_tables::mixed_overlap_10::routes, ct_tables::mixed_overlap_10::probes>();
}

TEST(RouteCtTables, MixedOverlap100)
{
	ct_tables::check<ct_tables::mixed_overlap_100::routes, ct_tables::mixed_overlap_100::probes>();
}

TEST(RouteCtTables, MixedOverlap1000)
{
	ct_tables::check<ct_tables::mixed_overlap_1000::routes, ct_tables::mixed_overlap_1000::probes>();
}
