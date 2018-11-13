import os
import math
from argparse import ArgumentParser

MATRIX_FILE_NAМЕ = "covarianceMatrix.out"

def read_views(file_name: str):
    with open(file_name) as file:
        first_line= file.readline()
        second_line = file.readline().strip()
        array = [int(x) for x in second_line.split("|")]
        return array

def get_triangualar_array(views_path, views):
    array = []
    with open(os.path.join(views_path, MATRIX_FILE_NAМЕ)) as file:
        for line in file:
            tokens = line.strip().split(",")
            view_id = int(tokens[0])
            view_offset = int(tokens[1])
            array.append(views[view_id][view_offset])
    return array

def get_uppper_cell_value(triangular_array, n: int, row_idx: int, col_idx: int):
    idx_cur  = n - row_idx + 1
    offset = col_idx - row_idx
    #print("idx_cur: {} offset: {}".format(idx_cur, offset))
    idx = int(n * (n + 1) / 2 - idx_cur  * (idx_cur + 1) / 2)
    #print(idx)
    #print(triagnular_array[idx + offset])
    return triagnular_array[idx + offset]

def get_cell_value(triagnular_array, n: int, row: int, col: int):
    if row > col:
        (row, col) = (col, row)
    return get_uppper_cell_value(triagnular_array, n, row, col)


if __name__ == "__main__":

    parser = ArgumentParser()
    parser.add_argument("-p", "--path", dest="path", required=True)

    args = parser.parse_args()
    #print(args.path)
    runtime_path = args.path
    views = {}

    for file_name in os.listdir(runtime_path):
        if file_name.endswith(".tbl"):
            view = read_views(os.path.join(runtime_path, file_name))
            #print(read_views(file_name))
            views[int(file_name.split(".")[0][1:])] = view
    #print(views)

    triagnular_array = get_triangualar_array(runtime_path, views)
    #print (triagnular_array)
    n = int(math.floor(math.sqrt(len(triagnular_array) * 2)))
    print("{0} {0}".format(n))
    for row in range (1, n + 1):
        for col in range(1, n + 1):
            val = get_cell_value(triagnular_array, n, row, col)
            print("{:>2}".format(val), end=" ")
        print()


