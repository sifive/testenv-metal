#include "metal/machine.h"
#include "unity_fixture.h"


//-----------------------------------------------------------------------------
// Unit test main
//-----------------------------------------------------------------------------

static void
_ut_run(void)
{
    UnityFixture.Verbose = 1;
    // UnityFixture.GroupFilter = "dma_sha";
    // UnityFixture.NameFilter = "irq";

    RUN_TEST_GROUP(trng);
    RUN_TEST_GROUP(dma_sha);
    // RUN_TEST_GROUP(dma_aes);
}

int main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, _ut_run);
}
