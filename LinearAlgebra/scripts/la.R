library(ade4)
library(data.table)
library(stringr)

args <- commandArgs(trailingOnly=TRUE)

data_path = args[1]
data_operation = args[2]
feat_num = as.integer(args[3])
cat_feat_num = as.integer(args[4])
feats = args[5:(feat_num+4)]
cat_feats = args[(feat_num+5):length(args)]

dataA <- read.csv(data_path, header=FALSE, sep='|')
#cat_feats <- c("rain")

dfA = as.data.frame(dataA)
#data_path
#data_operation
#feat_num
#cat_feat_num
#feats
#cat_feats

colnames(dfA) <-feats

for (feat in cat_feats)
{
    one_hot_col = acm.disjonctif(dfA[feat])
    dfA[feat] = NULL
    dfA = cbind(dfA, one_hot_col)
}

for (feat in cat_feats)
{
    #print(paste('feat', feat))
    for (col in colnames(dfA))
    {
        #print(paste('col', col))
        regex <-paste("^", feat, "([.]*)", sep='')
        #print(str_detect(col, regex))
        if (str_detect(col, regex))
        {
            print(paste("Deleted", col))
            dfA[col] = NULL
            break
        }
    }
}
print(ncol(dfA))
#dfA[1, 1:ncol(dfA)]

x <- c("what","is","truth")

if(data_operation == "svd") {
   dfA.svd <- svd(dfA)
   dfA.svd$d
} else if (data_operation == "qr") {
    QR <- qr(dfA)
    Q <- qr.Q(QR)
    R <- qr.R(QR)
    nrow(R)
    ncol(R)
} else {
   print("No truth found");
}
