#include <stdio.h>
#include "QRDecompTests.h"
#include "SVDecompTests.h"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
