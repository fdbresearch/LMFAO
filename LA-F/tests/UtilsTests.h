#ifndef _LMFAO_UTILS_TEST_H_
#define _LMFAO_UTILS_TEST_H_

#include <gtest/gtest.h>

const std::string TEST_PATH = "tests/data/";
const std::string TEST_FILE_IN = "test.in";

std::string getTestPath(uint32_t idx)
{
    return TEST_PATH + "test" + std::to_string(idx) + "/";
}
#endif
