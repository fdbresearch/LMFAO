#include <Eigen/Dense>
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cusolverDn.h>
#include <iostream> 
#include <iomanip> 

void gpuAssert(cudaError_t code, const char *file, int line, bool abort = true)
{
	if (code != cudaSuccess)
	{
		fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
		if (abort) { exit(code); }
	}
}


extern "C" void gpuErrchk(cudaError_t ans) { gpuAssert((ans), __FILE__, __LINE__); }

static const char *_cusolverGetErrorEnum(cusolverStatus_t error)
{
	switch (error)
	{
	case CUSOLVER_STATUS_SUCCESS:
		return "CUSOLVER_SUCCESS";

	case CUSOLVER_STATUS_NOT_INITIALIZED:
		return "CUSOLVER_STATUS_NOT_INITIALIZED";

	case CUSOLVER_STATUS_ALLOC_FAILED:
		return "CUSOLVER_STATUS_ALLOC_FAILED";

	case CUSOLVER_STATUS_INVALID_VALUE:
		return "CUSOLVER_STATUS_INVALID_VALUE";

	case CUSOLVER_STATUS_ARCH_MISMATCH:
		return "CUSOLVER_STATUS_ARCH_MISMATCH";

	case CUSOLVER_STATUS_EXECUTION_FAILED:
		return "CUSOLVER_STATUS_EXECUTION_FAILED";

	case CUSOLVER_STATUS_INTERNAL_ERROR:
		return "CUSOLVER_STATUS_INTERNAL_ERROR";

	case CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED:
		return "CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED";

	}

	return "<unknown>";
}


inline void __cusolveSafeCall(cusolverStatus_t err, const char *file, const int line)
{
	if (CUSOLVER_STATUS_SUCCESS != err) {
		fprintf(stderr, "CUSOLVE error in file '%s', line %d, error: %s \nterminating!\n", __FILE__, __LINE__, \
			_cusolverGetErrorEnum(err)); \
			assert(0); \
	}
}

extern "C" void cusolveSafeCall(cusolverStatus_t err) { __cusolveSafeCall(err, __FILE__, __LINE__); }

namespace LMFAO {
    namespace LinearAlgebra{
        void svdCuda(const Eigen::MatrixXd& a)
        {
           	// --- gesvd only supports Nrows >= Ncols
            // --- column major memory ordering

            int Nrows = a.rows();
            int Ncols = a.cols();

            // --- cuSOLVE input/output parameters/arrays
            int work_size = 0;
            int *devInfo;			gpuErrchk(cudaMalloc(&devInfo,	        sizeof(int)));
            
            // --- CUDA solver initialization
            cusolverDnHandle_t solver_handle;
            cusolverDnCreate(&solver_handle);

            // --- Setting the host, Nrows x Ncols matrix
            double *h_A = (double *)malloc(Nrows * Ncols * sizeof(double));
            for(int j = 0; j < Nrows; j++)
                for(int i = 0; i < Ncols; i++)
                {
                    //h_A[j + i*Nrows] = (i + j*j) * sqrt((double)(i + j));
                    h_A[j + i*Nrows] = a(i, j);
                }
            // --- Setting the device matrix and moving the host matrix to the device
            double *d_A;			gpuErrchk(cudaMalloc(&d_A,		Nrows * Ncols * sizeof(double)));
            gpuErrchk(cudaMemcpy(d_A, h_A, Nrows * Ncols * sizeof(double), cudaMemcpyHostToDevice));

            // --- host side SVD results space
            double *h_U = (double *)malloc(Nrows * Nrows     * sizeof(double));
            double *h_V = (double *)malloc(Ncols * Ncols     * sizeof(double));
            double *h_S = (double *)malloc(min(Nrows, Ncols) * sizeof(double));

            // --- device side SVD workspace and matrices
            double *d_U;			gpuErrchk(cudaMalloc(&d_U,	Nrows * Nrows     * sizeof(double)));
            double *d_V;			gpuErrchk(cudaMalloc(&d_V,	Ncols * Ncols	  * sizeof(double)));
            double *d_S;			gpuErrchk(cudaMalloc(&d_S,	min(Nrows, Ncols) * sizeof(double)));
            
            std::cout << "Init" << std::endl;
            // --- CUDA SVD initialization
            cusolveSafeCall(cusolverDnDgesvd_bufferSize(solver_handle, Nrows, Ncols, &work_size));
            double *work;	gpuErrchk(cudaMalloc(&work, work_size * sizeof(double)));
            
            std::cout << "Exec" << std::endl;
            // --- CUDA SVD execution
            cusolveSafeCall(cusolverDnDgesvd(solver_handle, 'A', 'A', Nrows, Ncols, d_A, Nrows, d_S, d_U, Nrows, d_V, Ncols, work, work_size, NULL, devInfo));
            int devInfo_h = 0;	gpuErrchk(cudaMemcpy(&devInfo_h, devInfo, sizeof(int), cudaMemcpyDeviceToHost));
            if (devInfo_h != 0) std::cout	<< "Unsuccessful SVD execution\n\n";
            std::cout << "Copy" << std::endl;
            // --- Moving the results from device to host
            gpuErrchk(cudaMemcpy(h_S, d_S, min(Nrows, Ncols) * sizeof(double), cudaMemcpyDeviceToHost));
            gpuErrchk(cudaMemcpy(h_U, d_U, Nrows * Nrows     * sizeof(double), cudaMemcpyDeviceToHost));
            gpuErrchk(cudaMemcpy(h_V, d_V, Ncols * Ncols     * sizeof(double), cudaMemcpyDeviceToHost));

            std::cout << "Singular values\n";
            for(int i = 0; i < min(Nrows, Ncols); i++) 
                std::cout << "d_S["<<i<<"] = " << std::setprecision(15) << h_S[i] << std::endl;

            std::cout << "\nLeft singular vectors - For y = A * x, the columns of U span the space of y\n";
            for(int j = 0; j < Nrows; j++) {
                printf("\n");
                for(int i = 0; i < Nrows; i++)
                    printf("U[%i,%i]=%f\n",i,j,h_U[j*Nrows + i]);
            }

            std::cout << "\nRight singular vectors - For y = A * x, the columns of V span the space of x\n";
            for(int i = 0; i < Ncols; i++) {
                printf("\n");
                for(int j = 0; j < Ncols; j++)
                    printf("V[%i,%i]=%f\n",i,j,h_V[j*Ncols + i]);
            }

            cusolverDnDestroy(solver_handle);
        }
    }
}

