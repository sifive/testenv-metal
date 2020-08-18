#include "metal/machine.h"
#include "unity_fixture.h"


//-----------------------------------------------------------------------------
// Unit test main
//-----------------------------------------------------------------------------

static void
_ut_run(void)
{
    UnityFixture.Verbose = 1;
    UnityFixture.GroupFilter = "dma_sha";
    //UnityFixture.NameFilter = "sha";

    RUN_TEST_GROUP(trng);
    RUN_TEST_GROUP(dma_sha_poll);
    RUN_TEST_GROUP(dma_sha_irq);
    RUN_TEST_GROUP(dma_aes_poll);
    RUN_TEST_GROUP(dma_aes_irq);
}

int main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, _ut_run);
}
