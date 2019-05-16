from argparse import ArgumentParser

import numpy as np
import pandas as pd
from sklearn.base import BaseEstimator, TransformerMixin
from sklearn.preprocessing import OneHotEncoder
from sklearn.compose import ColumnTransformer
import sys
from timeit import default_timer as timer
import os

class DummyEncoder(BaseEstimator, TransformerMixin):
    def transform(self, X):
        ohe = OneHotEncoder(handle_unknown='ignore')
        return ohe.fit_transform(X)[:,1:]

    def fit(self, X, y=None, **fit_params):
        return self


class IdentityTransformer(BaseEstimator, TransformerMixin):
    def transform(self, input_array):
        return input_array*1

    def fit(self, X, y=None, **fit_params):
        return self


def dump_qr(r, dump_file):
    diag_sgn_r = np.sign(r.diagonal())
    matr_sgn_r = np.diag(diag_sgn_r)
    r_positive = np.dot(matr_sgn_r, r)

    with open(dump_file, "w") as file:
        #print(r_positive.shape[0], r_positive.shape[1])
        file.write("{} {}\n".format(r_positive.shape[0], r_positive.shape[1]))
        row_num = r_positive.shape[0]
        col_num = r_positive.shape[1]
        for row in range(0, row_num):
            for col in range(0, col_num):
                file.write("{} ".format(r_positive[row, col]))
            file.write("\n")

def run_test(data, columns, columns_cat, dump, dump_file):
    start = timer()
    transformer_a = []
    for column in columns:
        if column in columns_cat:
            transf_name = column + "_onehot"
            transformer_a.append((transf_name, DummyEncoder(), [column]))
        else:
            transf_name = column + "_identity"
            transformer_a.append((transf_name, IdentityTransformer(), [column]))

    preprocessor = ColumnTransformer(transformers=transformer_a)
    one_hot_a = preprocessor.fit_transform(data)
    if operation == 'svd':
        _, s, vh = np.linalg.svd(one_hot_a, full_matrices=False)
        print(s)
    elif operation == 'qr':

        #If A is invertible, then the factorization is unique if we require the diagonal elements of R to be positive.
        # If A is of full rank n and we require that the diagonal elements of R1 are positive then R1 and Q1 are unique.
        # Make elements on diagonal postive by multiplying diagonal R by a diagonal  matrix
        #  whose diagonal is sign of diagonal of R
        r = np.linalg.qr(one_hot_a, mode='r')
        if dump:
            dump_qr(r, dump_file)

    end = timer()
    print(end - start)


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("-f", "--features", dest="features", required=True)
    parser.add_argument("-c", "--categorical_features", dest="categorical_features", required=True)
    parser.add_argument("-d", "--data_path", dest="data_path", required=True)
    parser.add_argument("-o", "--operation", dest="operation", required=True)
    parser.add_argument("-n", "--num_it", dest="num_it", required=True)
    parser.add_argument("-D", "--dump_file", dest="dump_file", required=False)
    parser.add_argument('--dump', dest="dump", action='store_true')

    np.set_printoptions(threshold=sys.maxsize, precision=20)
    pd.set_option('display.max_columns', 500)
    args = parser.parse_args()
    features = args.features
    cat_featurs = args.categorical_features
    data_path = args.data_path
    operation = args.operation
    num_it = int(args.num_it)
    dump_file = args.dump_file
    dump = args.dump if args.dump else False
    columns = features.split(",")
    columns_cat = cat_featurs.split(",")

    data = pd.read_csv(data_path, names=columns, delimiter="|", header=None)

    for it in range(0, num_it):
        run_test(data, columns, columns_cat, dump, dump_file)
