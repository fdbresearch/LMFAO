import os
import math
import pandas as pd
from argparse import ArgumentParser

def evaluate_path(file_path: str, col_idx: int):
    data_frame = pd.read_csv(file_path, sep='|', header=None)
    v_set_elems = []

    col_num = len(data_frame.columns)
    col_a = [col_idx] if col_idx != -1 else range(col_num)
    for col in range(col_num):
        v_set_elems.append(set())

    for index, row in data_frame.iterrows():
        for col in col_a:
            val = row[int(col)]
            v_set_elems[col].add(val)

    print(file_path)
    for col in range(col_num):
        print("Idx: {} Len: {}".format(col, len(v_set_elems[col])))
        print(v_set_elems[col])


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("-i", "--input_path", dest="input_path", required=True)
    parser.add_argument("-c", "--column", dest="column", required=True)
    args = parser.parse_args()
    input_path = args.input_path
    column_idx = int(args.column)

    if os.path.isdir(input_path):
        for filename in os.listdir(input_path):
            if filename.endswith(".tbl"):
                file_path = os.path.join(input_path, filename)
                evaluate_path(file_path, column_idx)

    else:
        evaluate_path(input_path, column_idx)


