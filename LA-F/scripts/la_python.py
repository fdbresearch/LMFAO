from argparse import ArgumentParser

import numpy as np
import scipy as sp
import pandas as pd
import scipy
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
    #If A is invertible, then the factorization is unique if we require the diagonal elements of R to be positive.
    # If A is of full rank n and we require that the diagonal elements of R1 are positive then R1 and Q1 are unique.
    # Make elements on diagonal postive by multiplying diagonal R by a diagonal  matrix
    #  whose diagonal is sign of diagonal of R
    diag_sgn_r = np.sign(r.diagonal())
    matr_sgn_r = np.diag(diag_sgn_r)
    r_positive = np.dot(matr_sgn_r, r)

    with open(dump_file, "w") as file:
        file.write("{} {}\n".format(r_positive.shape[0], r_positive.shape[1]))
        row_num = r_positive.shape[0]
        col_num = r_positive.shape[1]
        print(row_num)
        print(col_num)
        for row in range(0, row_num):
            for col in range(0, col_num):
                file.write("{} ".format(r_positive[row, col]))
            file.write("\n")

def dump_sigma(sigma, dump_file):
    with open(dump_file, "w") as file:
        n = sigma.shape[0]
        file.write("{}\n".format(sigma.shape[0]))
        for idx in range(0, n):
            file.write("{}\n".format(sigma[idx]))


def run_test(linalg_sys, data, columns, columns_cat, dump, dump_file, operation, sin_vals):
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

    if linalg_sys == 'numpy':
        print('numpy')
        if operation == 'svd':
            if sin_vals:
                sigma = np.linalg.svd(one_hot_a, full_matrices=False,
                                        compute_uv=False)
            else:
                _, sigma, _ = np.linalg.svd(one_hot_a, full_matrices=False,
                                        compute_uv=True)
            if dump:
                dump_sigma(sigma, dump_file)
        elif operation == 'qr':
            # Numpy offers a mode in which is possible only to get reduced R.
            # This gives the fastest processing time because of avoiding copying
            # the data.
            r = np.linalg.qr(one_hot_a, mode='r')
            if dump:
                dump_qr(r, dump_file)
    elif linalg_sys == 'scipy':
            print('scipy')
            if operation == 'svd':
                if sin_vals:
                    sigma = sp.linalg.svd(one_hot_a, full_matrices=False,
                                           overwrite_a=True, check_finite=False,
                                           compute_uv=False)
                else:
                    _, sigma, _ = sp.linalg.svd(one_hot_a, full_matrices=False,
                                            overwrite_a=True, check_finite=False,
                                            compute_uv=True)
                if dump:
                    dump_sigma(sigma, dump_file)
            elif operation == 'qr':
                # The problem with scipy is that it doesn't offer a mode in which
                # reduced R is returned as in numpy, which results in larger processing time
                # mainly because of copying.
                r, = scipy.linalg.qr(one_hot_a, overwrite_a=True, mode='r', check_finite=False)
                r = np.resize(r, (r.shape[1], r.shape[1]))
                if dump:
                    dump_qr(r, dump_file)

    end = timer()
    print("##LMFAO##Calculation##{}".format(end - start))
    return end - start


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("-s", "--system", dest="linalg_sys", required=True)
    parser.add_argument("-f", "--features", dest="features", required=True)
    parser.add_argument("-c", "--categorical_features", dest="categorical_features", required=True)
    parser.add_argument("-d", "--data_path", dest="data_path", required=True)
    parser.add_argument("-o", "--operation", dest="operation", required=True)
    parser.add_argument("-n", "--num_it", dest="num_it", required=True)
    parser.add_argument("-D", "--dump_file", dest="dump_file", required=False)
    parser.add_argument('--dump', dest="dump", action='store_true')
    parser.add_argument("--sin_vals", dest="sin_vals", required=True)

    np.set_printoptions(threshold=sys.maxsize, precision=20)
    pd.set_option('display.max_columns', 500)
    args = parser.parse_args()
    linalg_sys = args.linalg_sys
    features = args.features
    cat_featurs = args.categorical_features
    data_path = args.data_path
    operation = args.operation
    num_it = int(args.num_it)
    dump_file = args.dump_file
    dump = args.dump if args.dump else False
    sin_vals = True if args.sin_vals == 'true' else False
    columns = features.split(",")
    columns_cat = cat_featurs.split(",")

    data = pd.read_csv(data_path, names=columns, delimiter="|", header=None)

    cum_time = 0.0
    for it in range(0, num_it):
        time = run_test(linalg_sys, data, columns, columns_cat, dump, dump_file, operation, sin_vals)
        cum_time += time
    print(cum_time / num_it)
