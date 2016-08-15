#ifndef DDRF_CUDA_COORDINATES_H_
#define DDRF_CUDA_COORDINATES_H_

namespace ddrf
{
    namespace cuda
    {
        __device__ auto coord_x() -> unsigned int
        {
            return blockIdx.x * blockDim.x + threadIdx.x;
        }

        __device__ auto coord_y() -> unsigned int
        {
            return blockIdx.y * blockDim.y + threadIdx.y;
        }

        __device__ auto coord_z() -> unsigned int
        {
            return blockIdx.z * blockDim.z + threadIdx.z;
        }
    }
}

#endif /* DDRF_CUDA_COORDINATES_H_ */
