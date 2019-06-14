
# Installs missing packages
list.of.packages <- c("ade4", "data.table", 'stringr', 'pracma', 'fastDummies')
new.packages <- list.of.packages[!(list.of.packages %in% installed.packages()[,"Package"])]
if(length(new.packages)) install.packages(new.packages)

library(ade4)
library(data.table)
library(stringr)
library(pracma)
library(fastDummies)

args <- commandArgs(trailingOnly=TRUE)

data_path = args[1]
data_operation = args[2]
feat_num = as.integer(args[3])
cat_feat_num = as.integer(args[4])
num_it = as.integer(args[5])
dump = as.logical(args[6])
dump_file = args[7]
sin_vals = as.logical(args[8])
feats = args[9:(feat_num+8)]
cat_feats = args[(feat_num+9):length(args)]
sin_vals
dataA <- read.csv(data_path, header=FALSE, sep='|')
#cat_feats <- c("rain")
#parser.add_argument("-D", "--dump_file", dest="dump_file", required=False)
#parser.add_argument('--dump', dest="dump", action='store_true')

#dump
#data_path
#data_operation
#feat_num
#cat_feat_num
#feats
#cat_feats

# TODO: Optimize one hot encoding
for (it in 1:num_it){
    dfA = as.data.frame(dataA)

    colnames(dfA) <-feats
    start_time <- Sys.time()
    #dfT <- one_hot(as.data.table(dfA), cols=cat_feats)
    #factor_columns <- names(which(lapply(train, class) == "factor"))
    #factor_predictors <- setdiff(factor_columns, c("sold", "UniqueID"))
    #dummies_formula <- as.formula(paste("~ ", paste0(factor_predictors, collapse=" + ")))
    #dfA <- dummyVars(" ~ .", data=dfA,  fullRank=TRUE)
    dfA = dummy_cols(dfA, select_columns=cat_feats,
                     remove_first_dummy=FALSE)

    # Removes old columns of of ohe columns.
    for (feat in cat_feats)
    {

        #one_hot_col = acm.disjonctif(dfA[feat])
        dfA[feat] = NULL
        #dfA = cbind(dfA, one_hot_col)
    }
    #print(colnames(dfA))
    for (feat in cat_feats)
    {
        #print(paste('feat', feat))
        min_col_id = 99999999
        min_col = ''
        for (col in colnames(dfA))
        {
            #print(paste('col', col))
            # Delets one hot encoded values
            regex <-paste("^", feat, "([.]*)", sep='')

            #print(str_detect(col, regex))

            if (str_detect(col, regex))
            {
                col_num = as.integer(sub(paste(feat, "_", sep=''), "", col))
                if (col_num < min_col_id)
                {
                    print(paste('Number', col_num))
                    min_col_id = col_num
                    min_col = col
                }
            }
        }
        print(paste("Deleted", min_col))
        dfA[min_col] = NULL
    }
    end_time <- Sys.time()
    diff <- difftime(end_time, start_time, units='secs')
    cat(paste('##LMFAO##One hot encode##', diff), '\n')
    print(ncol(dfA))
    #dfA[1, 1:ncol(dfA)]

    start_time <-Sys.time()
    if(data_operation == "svd") {
        if (sin_vals)
        {
            dfA.svd <- svd(dfA, nu=0, nv=0, LINPACK=FALSE)
        }
        else
        {
            dfA.svd <- svd(dfA, LINPACK=FALSE)
        }
        if (dump) {
            file.create(dump_file)
            fprintf("%d\n", length(dfA.svd$d), file=dump_file, append=TRUE)
            for (value in dfA.svd$d) {
                fprintf("%.15f\n",value, file=dump_file, append=TRUE)
            }
        }
    }
    else if (data_operation == "qr") {
        QR <- qr(dfA, LAPACK=TRUE)
        #Q <- qr.Q(QR)
        R <- qr.R(QR)

        if (dump) {
            R <- R[, order(QR$pivot)]
            #print(R)
            #TODO: Recheck these parts.
            sgn <- sign(diag(R))
            R <- diag(sgn) %*% R
            file.create(dump_file)
            fprintf("%d %d\n", nrow(R), ncol(R), file=dump_file, append=TRUE)
            for (row in 1:nrow(R)) {
                for (col in 1:ncol(R)) {
                    fprintf("%.15f ", R[row, col], file=dump_file, append=TRUE)
                }
                fprintf("\n", file=dump_file, append=TRUE)
            }
        }
    }
    else {
       print("No truth found");
    }
    end_time <- Sys.time()
    diff <- difftime(end_time, start_time, units='secs')
    cat(paste('##LMFAO##Calculate##', diff), '\n')
}
