library(ade4)
library(data.table)
library(stringr)

args <- commandArgs(trailingOnly=TRUE) 

dataA <- read.csv('/media/popina/test/dfdb/LMFAO/runtime/sql/joinresult.txt', header=FALSE, sep='|')
cat_feats <- c("rain")

dfA = as.data.frame(dataA)

feat_num = as.integer(args[1])
cat_feat_num = as.integer(args[2])
feats = args[3:(feat_num+2)]
cat_feats = args[(feat_num+3):length(args)]
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

start_time <- Sys.time()
dfA.svd <- svd(dfA)
end_time <- Sys.time()
end_time - start_time
dfA.svd$d

