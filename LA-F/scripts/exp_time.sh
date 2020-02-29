# Just update -d so it contains all tests 
# Add -p=password for that user. 
./runTests.sh --build=m -d=usretailer_35f_1 -o=qr,svd -r=/home/popina/LMFAO  --run  --time --full_exps --clean
./runTests.sh --build=l -d=usretailer_35f_1 -o=qr_chol,qr_mul_t,svd_qr,svd_qr_chol,svd_eig_dec,svd_alt_min -r=/home/popina/LMFAO  --run  --time --full_exps
# --sin_vals used in R, numpy and scipy
./runTests.sh --build=jsnre -d=usretailer_35f_1 -o=qr,svd -r=/home/popina/LMFAO  --run  --time --full_exps --sin_vals 
