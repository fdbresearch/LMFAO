from argparse import ArgumentParser

import numpy as np
import pandas as pd
from scipy import linalg
#from scipy.sparse import  linalg
from sklearn.base import BaseEstimator, TransformerMixin
from sklearn.preprocessing import OneHotEncoder
from sklearn.compose import ColumnTransformer

JOIN_RES_PATH = "/media/popina/test/dfdb/LMFAO/runtime/sql/joinresult.txt"


class DummyEncoder(BaseEstimator, TransformerMixin):
    def transform(self, X):
        ohe = OneHotEncoder(handle_unknown='ignore')
        return ohe.fit_transform(X)[:,1:]

    def fit(self, X, y=None, **fit_params):
        return self


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("-f", "--features", dest="features", required=True)
    parser.add_argument("-c", "--categorical_features", dest="categorical_features", required=True)

    args = parser.parse_args()
    features = args.features
    cat_featurs = args.categorical_features
    columns = features.split(",")

    columns_cat = cat_featurs.split(",")

    transformer_a = []
    data = pd.read_csv(JOIN_RES_PATH, names=columns, delimiter="|", header=None)
    for column in columns_cat:
        transf_name = column + "_onehot"
        transformer_a.append((transf_name, DummyEncoder(), [column]))

    preprocessor = ColumnTransformer(transformers=transformer_a, remainder='passthrough')
    one_hot_a = preprocessor.fit_transform(data)
    u, s, vh = linalg.svd(one_hot_a, full_matrices=False)
    #u, s, vh = linalg.svds(one_hot_a, k=len(columns))
    print(s)
