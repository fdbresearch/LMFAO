import os
import math
from argparse import ArgumentParser

MATRIX_FILE_NAМЕ = "covarianceMatrix.out"
FEATURES_FILE_NAME = "features.conf"
TREE_DECOMP_FILE_NAME = "treedecomposition.conf"

def parse_features_order(data_path: str):
    features = []
    features_name_idx = {}

    tree_decomp_file_name = os.path.join(data_path, TREE_DECOMP_FILE_NAME)
    features_file_name = os.path.join(data_path, FEATURES_FILE_NAME)
    tree_decomp_file_name
    # Parse treedecomoposition.conf for variable order.
    with open(tree_decomp_file_name) as file:
        line = file.readline()
        while (line.startswith('#')):
            line = file.readline()
        features_count = int(line.strip().split(' ')[0])
        for idx in range(0, features_count):
            line = file.readline()
            #print(line)
            feature_id_name = {'id': int(line.strip().split(' ')[0]), 
                               'name':  line.strip().split(' ')[1]}

            features_name_idx[feature_id_name['name']] = feature_id_name['id']
            features.append({
                'name': feature_id_name['name'], 
                'is_cat': False,
                'domain_size': 0,
                'is_valid': False})

    # Parse features.conf for type of features and to see which features are included
    with open(features_file_name) as file:
        line = file.readline()
        while (line.startswith('#')):
            line = file.readline()
        features_count = int(line.strip().split(',')[0].strip())
        cnt = 0
        while True:
            line = file.readline()
            if not line:
                break
            if line.strip().startswith('#'):
                continue
            feature_name_is_cat = {'name': line.strip().split(':')[0], 
                               'is_cat':  int(line.strip().split(':')[1]) != 0,
                               'domain_size': int(line.strip().split(':')[1])}
            feature_idx = features_name_idx[feature_name_is_cat['name']]
            features[feature_idx]['is_cat'] = feature_name_is_cat['is_cat']
            features[feature_idx]['domain_size'] = feature_name_is_cat['domain_size']
            features[features_name_idx[feature_name_is_cat['name']]]['is_valid'] = True
            cnt += 1
    features = [feature for feature in features if feature['is_valid']]
    #print(features)
    return features

def read_view(file_name: str):
    #print(file_name)
    with open(file_name) as file:
        first_line = file.readline()
        value_dimension = int(first_line.strip().split(" ")[0])
        num_elements = int(first_line.strip().split(" ")[1])

        array_line_splits = []
        while True:
            second_line = file.readline().strip()
            if not second_line:
                break
            array_line_splits.append(second_line.split("|"))
   
    vectors = []
    for idx_elem in range(0, num_elements):
        vector = {}
        for line in array_line_splits:
            if value_dimension == 0:
                tuple_key = ()
            elif value_dimension == 1:
                tuple_key = (float(line[0]), )
            elif value_dimension == 2:
                tuple_key = (float(line[0]), float(line[1]))
            vector[tuple_key] = float(line[idx_elem + value_dimension])
        vectors.append(vector)
    #print(vectors)

    return vectors            


def get_triangualar_array(views_path, views):
    array = []
    with open(os.path.join(views_path, MATRIX_FILE_NAМЕ)) as file:
        for line in file:
            tokens = line.strip().split(",")
            view_id = int(tokens[0])
            view_offset = int(tokens[1])
            array.append(views[view_id][view_offset])
    return array


def get_uppper_cell_value(triangular_array, n: int, row_idx: int, col_idx: int, 
                         cat_features_cnt: list, features):
    if (row_idx == 0) and (col_idx == 0):
        return triangular_array[0]
    #print("Before row_idx: {} col_idx:{}".format(row_idx, col_idx))

    if (row_idx == 0):
        row_idx = col_idx - 1
        col_idx = row_idx
        # Check categorical count for the current row
        # (2nd feature maps to 1st row in the new design, thus row_idx stays)
        # row_idx + 1 because first feature is "skipped" (continuous) in changed matrix.
        offset_cat = cat_features_cnt[row_idx + 1]
        #("Because IF * IF is on the right place" for the )
        if features[row_idx + 1]['is_cat']:
            offset_cat -= 1
    else: 
        row_idx = row_idx - 1
        offset_cat = cat_features_cnt[row_idx+1]
    
    # The part above is used to make mapping possible because 
    # value of triangular matrix are not outputted in the right order
    # They are sorted IF^2 IF * F1 F1^2 F1 * F2 F1 * F3 ... F1 * FN
    #                      IF * F2 F2^2 F2 * F3   ....      F2 * FN
    #                                               IF * FN FN^2
    # When we omit IF^2 and sort data, we get triangular matrix of size n (n+1 is passed to the 
    # function), whose correct row is one above. 

    idx_cur = n - row_idx
    idx = int(n * (n + 1) / 2 - idx_cur * (idx_cur + 1) / 2)
    offset = col_idx - row_idx - offset_cat
    '''
    print("row_idx: {} col_idx:{} idx:{} offset:{} offset_cat{}".
            format(row_idx, col_idx, idx, offset, offset_cat))
    '''
    # We add + 1 because of "skipping" IF^2
    return triangular_array[idx + offset + 1]


def get_cell_value(triangular_array, n: int, row: int, col: int, cat_features_cnt: list,
                   features):
    if row > col:
        (row, col) = (col, row)
    return get_uppper_cell_value(triangular_array, n, row, col, cat_features_cnt, features)


def get_idx(domain_size_cnt, domains_shifts, feature_idx: int, category: int):
    return domain_size_cnt[feature_idx] + domains_shifts[feature_idx][category]


def is_first_cat(domain_shifts, feature_idx: int , category: int ):
    return domain_shifts[feature_idx][category] == -1


def print_cell(row, col, val, cell_type, domain_size_cnt, domains_shifts):
    if cell_type == 'c':
        print("{} {} {} 0".format(row + domain_size_cnt[row], col + domain_size_cnt[col], 
                                val[()]))
    elif cell_type == 'rv':
        for dom_val in val:
            f1_cat_dom = int(dom_val[0])
            if is_first_cat(domains_shifts, col, f1_cat_dom):
                continue
            idx = get_idx(domain_size_cnt, domains_shifts, col, f1_cat_dom)
            print("{} {} {} 1".format(row + domain_size_cnt[row] ,
                                    col + idx, val[dom_val]))
    elif cell_type == 'cv':
        for dom_val in val:
            f1_cat_dom = int(dom_val[0])
            if is_first_cat(domains_shifts, row, f1_cat_dom):
                continue
            idx = get_idx(domain_size_cnt, domains_shifts, row, f1_cat_dom)
            print("{} {} {} 1".format(row + idx,
                                    col + domain_size_cnt[col] , val[dom_val]))
    elif cell_type == 'd':
        #print(row, col)
        #print('matd{}'.format(val))
        for dom_val in val:
            f1_cat_dom = int(dom_val[0])
            if is_first_cat(domains_shifts, row, f1_cat_dom):
                continue
            idx = get_idx(domain_size_cnt, domains_shifts, row, f1_cat_dom)
            print("{} {} {} 1".format(row + idx, col + idx, val[dom_val]))

    elif cell_type == 'm':
        # Output of categorical variables is problematic
        #print(row, col)
        #print('matp{}'.format(val))
        for dom_val in val:
            f1_cat_dom = int(dom_val[0])
            f2_cat_dom = int(dom_val[1])
            if row > col:
                f1_cat_dom, f2_cat_dom = f2_cat_dom, f1_cat_dom
            if is_first_cat(domains_shifts, row, f1_cat_dom) or \
                    is_first_cat(domains_shifts, col, f2_cat_dom):
                continue

            idx1 = get_idx(domain_size_cnt, domains_shifts, row, f1_cat_dom)
            idx2 = get_idx(domain_size_cnt, domains_shifts, col, f2_cat_dom)
            print("{} {} {} 1".format(row + idx1, col + idx2, val[dom_val]))


def get_domains(triangular_array,  cat_features_cnt, features, n):
    domains = [None] * len(features)
    for idx in range(0, len(features)):
        domains[idx] = set()
    for row in range(0, n):
        for col in range(0, n):
            feature_row = features[row]
            feature_col = features[col]
            if (feature_col['is_cat']) and feature_row['is_cat'] and (row == col):
                val = get_cell_value(triangular_array, n, row, col, cat_features_cnt, features)
                for dom_val in val:
                    domains[row].add(dom_val[0])
    return domains


def get_cat_feat_cnt(features):
    cat_features_cnt = [0] * (len(features))
    for idx in range(1, len(features)):
        cat_features_cnt[idx] = cat_features_cnt[idx - 1]
        if features[idx]['is_cat']:
            cat_features_cnt[idx] += 1

    return cat_features_cnt


def get_domains_cnt(features, domains):
    domain_size_cnt = [0] * (len(features))
    for idx in range(1, len(features)):
        domain_size_cnt[idx] = domain_size_cnt[idx - 1] + len(domains[idx])
        if features[idx]['is_cat']:
            # We skip the first category because of linear dependence
            domain_size_cnt[idx] -= 1

    return domain_size_cnt


def get_category_shifts(features, domains):
    domain_idx = [None] * (len(features))
    for idx in range(1, len(features)):
        sort_dom = sorted(list(domains[idx]))
        # List of shifts for all categories in a domain.
        shift = {it: idx - 1 for (idx, it) in enumerate(sort_dom)}
        domain_idx[idx] =  shift

    return domain_idx

def parse_faqs(output_path: str, features):
    views = {}

    for file_name in os.listdir(output_path):
        if file_name.endswith(".tbl"):
            view = read_view(os.path.join(output_path, file_name))
            #print(read_view(file_name))
            views[int(file_name.split(".")[0][1:])] = view
    #print(views)


    triangular_array = get_triangualar_array(output_path, views)
    #print ("Len of trarray: {0}".format(len(triangular_array)))
    n = int(math.floor(math.sqrt(len(triangular_array) * 2)))
    #print("{0} {0}".format(n))

    features = [{
                'name': 'intercept', 
                'is_cat': False,
                'domain_size': 0,
                'is_valid': True}] + features

    #print(features)

    cat_features_cnt = get_cat_feat_cnt(features)
    domains = get_domains(triangular_array, cat_features_cnt, features, n)
    domain_size_cnt = get_domains_cnt(features, domains)
    cat_shifts = get_category_shifts(features, domains)
    #In order not to check domain size of previous in the print cell, we add this hack
    domain_size_cnt = [0] + domain_size_cnt
    #print(domain_size_cnt)




    # Domain_size_cnt represents all possible domain_cnt
    # Features expanded
    print('{:0}'.format(n + domain_size_cnt[-1]))
    # Features
    print('{:0}'.format(n))
    # Continuous
    print('{:0}'.format(n - cat_features_cnt[-1]))



    for row in range(0, n):
        for col in range(0, n):
            feature_row = features[row]
            feature_col  = features[col]
            #print(feature_row)
            #print(feature_col)
            val = get_cell_value(triangular_array, n, row, col, cat_features_cnt, features)
            cell_type = ''
            '''
            2 3    2 0 1 
            0 1  x 3 1 0
            1 0
            = 13  3  2 
              3   1  0
              2   0  1
            '''
            # Continuous * categorical is row vector.
            if (not feature_row['is_cat']) and (not (feature_col['is_cat'])):
                cell_type = 'c'
            elif (feature_row['is_cat']) and not (feature_col['is_cat']):
                cell_type = 'cv'
            elif (not feature_row['is_cat']) and feature_col['is_cat']:
                cell_type = 'rv'
            elif (feature_col['is_cat']) and feature_row['is_cat'] and (row == col):
                cell_type = 'd'
            else:
                cell_type = 'm'
            #print(row, col)
            print_cell(row, col, val, cell_type, domain_size_cnt, cat_shifts)
        #print()

''' 
For usage of this program, user should pass the argument path which represents 
path to data which should appropriate format generated by lmfao.
python3 transform.py -o /media/popina/test/dfdb/LMFAO/runtime/cpp/output -d /media/popina/test/dfdb/LMFAO/data/usretailer/
is an example.
It will print the matrix in required format to the output. 
'''
if __name__ == "__main__":

    parser = ArgumentParser()
    parser.add_argument("-o", "--output_path", dest="output_path", required=True)
    parser.add_argument("-d", "--data_path", dest="data_path", required=True)

    args = parser.parse_args()
    output_path = args.output_path
    data_path = args.data_path
    features = parse_features_order(data_path)
    parse_faqs(output_path, features)

