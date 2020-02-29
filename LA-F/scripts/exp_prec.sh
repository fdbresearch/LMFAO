# Just update -d so it contains all tests, and -o to contain all operations.
# Run LMFAO test for comparsion
./runTests.sh --build=l -d=usretailer_35f_1 -o=qr_chol,qr_mul_t -r=/home/popina/LMFAO  --run  --dump --clean
# Run all other tests
./runTests.sh --build=jsrnme -d=usretailer_35f_1 -o=qr -r=/home/popina/LMFAO  --run  --dump
# Once they are finished compare their precision to each of the approaches in LMFAO 
./runTests.sh --build=srnme -d=usretailer_35f_1 -o=qr_chol,qr_mul_t -r=/home/popina/LMFAO  --precision