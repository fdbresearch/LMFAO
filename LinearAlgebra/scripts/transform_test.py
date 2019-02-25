import subprocess
from subprocess import check_output
import unittest


TESTS_PATH = "/home/popina/Documents/FDB/LMFAO/LinearAlgebra/scripts/tests/converter"


class TestOrderCont(unittest.TestCase):
    def setUp(self):
        self.data_path = TESTS_PATH + "/test1/data_path"
        self.output_path = TESTS_PATH + "/test1/output_path"
        self.expected_path = TESTS_PATH + "/test1/expected.out"

    def testEvaluation(self):
        output = check_output(['python3', 'transform.py',
                         '-o' + self.output_path, '-d' + self.data_path]).decode('utf-8')
        lines = output.split("\n")
        with open(self.expected_path, 'r') as expected_f:
            for line in lines:
                line_exp = expected_f.readline().strip()
                self.assertEqual(line, line_exp)


class TestOrderCat(unittest.TestCase):
    def setUp(self):
        self.data_path = TESTS_PATH + "/test2/data_path"
        self.output_path = TESTS_PATH + "/test2/output_path"
        self.expected_path = TESTS_PATH + "/test2/expected.out"

    def testEvaluation(self):
        output = check_output(['python3', 'transform.py',
                         '-o' + self.output_path, '-d' + self.data_path]).decode('utf-8')
        lines = output.split("\n")
        with open(self.expected_path, 'r') as expected_f:
            for line in lines:
                line_exp = expected_f.readline().strip()
                self.assertEqual(line, line_exp)


class TestOrderCat2(unittest.TestCase):
    def setUp(self):
        self.data_path = TESTS_PATH + "/test3/data_path"
        self.output_path = TESTS_PATH + "/test3/output_path"
        self.expected_path = TESTS_PATH + "/test3/expected.out"

    def testEvaluation(self):
        output = check_output(['python3', 'transform.py',
                         '-o' + self.output_path, '-d' + self.data_path]).decode('utf-8')
        lines = output.split("\n")
        with open(self.expected_path, 'r') as expected_f:
            for line in lines:
                line_exp = expected_f.readline().strip()
                self.assertEqual(line, line_exp)


class TestOrderCatDomainSize(unittest.TestCase):
    def setUp(self):
        self.data_path = TESTS_PATH + "/test4/data_path"
        self.output_path = TESTS_PATH + "/test4/output_path"
        self.expected_path = TESTS_PATH + "/test4/expected.out"

    def testEvaluation(self):
        output = check_output(['python3', 'transform.py',
                         '-o' + self.output_path, '-d' + self.data_path]).decode('utf-8')
        lines = output.split("\n")
        with open(self.expected_path, 'r') as expected_f:
            for line in lines:
                line_exp = expected_f.readline().strip()
                self.assertEqual(line, line_exp)


class TestOrder2CatDomainSize(unittest.TestCase):
    def setUp(self):
        self.data_path = TESTS_PATH + "/test5/data_path"
        self.output_path = TESTS_PATH + "/test5/output_path"
        self.expected_path = TESTS_PATH + "/test5/expected.out"

    def testEvaluation(self):
        output = check_output(['python3', 'transform.py',
                         '-o' + self.output_path, '-d' + self.data_path]).decode('utf-8')
        lines = output.split("\n")
        with open(self.expected_path, 'r') as expected_f:
            for line in lines:
                line_exp = expected_f.readline().strip()
                self.assertEqual(line, line_exp)


if __name__ == "__main__":
    unittest.main()
