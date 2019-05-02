library(ade4)
library(data.table)
library(stringr)

args <- commandArgs(trailingOnly=TRUE) 

data_path = args[1]
feat_num = as.integer(args[2])
cat_feat_num = as.integer(args[3])
feats = args[4:(feat_num+3)]
cat_feats = args[(feat_num+4):length(args)]

dataA <- read.csv(data_path, header=FALSE, sep='|')
#cat_feats <- c("rain")

dfA = as.data.frame(dataA)
#data_path
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

start_time <- Sys.time()
dfA.svd <- svd(dfA)
end_time <- Sys.time()
end_time - start_time
dfA.svd$d

