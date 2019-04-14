import numpy as np
import pandas as pd
from scipy import linalg
from sklearn.preprocessing import OneHotEncoder
from sklearn.compose import ColumnTransformer


JOIN_RES_PATH = "/media/popina/test/dfdb/LMFAO/runtime/sql/joinresult.txt"

if __name__ == "__main__":
    columns = ["inventoryunits", "rgn_cd", "clim_zn_nbr", "tot_area_sq_ft", "sell_area_sq_ft", "avghhi",
           "supertargetdistance", "supertargetdrivetime", "targetdistance", "targetdrivetime", "walmartdistance",
           "walmartdrivetime", "walmartsupercenterdistance", "walmartsupercenterdrivetime", "white", "asian",
           "pacific", "black", "medianage", "houseunits", "families", "households", "husbwife", "males", "females",
           "householdschildren", "hispanic", "category", "prize", "rain", "snow", "maxtemp", "mintemp", "meanwind",
           "thunder"]
    preprocessor = ColumnTransformer(
        transformers=[
            ('thunder_onehot', OneHotEncoder(handle_unknown='ignore'), ['thunder']),
            ('snow_onehot', OneHotEncoder(handle_unknown='ignore'), ['snow']),
            ('rain_onehot', OneHotEncoder(handle_unknown='ignore'), ['rain']),
            ('category_onehot', OneHotEncoder(handle_unknown='ignore'), ['category'])],
        remainder='passthrough')
    #a = np.loadtxt(open(JOIN_RES_PATH, "r"), delimiter="|")
    data = pd.read_csv(JOIN_RES_PATH, names=columns, delimiter="|", header=None)
    one_hot_a = preprocessor.fit_transform(data)
    one_hot_a = np.delete(one_hot_a, np.s_[0, 2, 4, 6], axis=1)
    u, s, vh = np.linalg.svd(one_hot_a, full_matrices=False)
    print(s)
