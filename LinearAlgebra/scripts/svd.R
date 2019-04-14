dataA <- read.csv('/media/popina/test/dfdb/LMFAO/runtime/sql/joinresult.txt', header=FALSE, sep='|')
A <- as.matrix(dataA)
start_time <- Sys.time()
A.svd <- svd(A)
end_time <- Sys.time()
end_time - start_time
A.svd$d

