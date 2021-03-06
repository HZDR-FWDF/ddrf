/*
 * This file is part of the GLADOS library.
 *
 * Copyright (C) 2016 Helmholtz-Zentrum Dresden-Rossendorf
 *
 * GLADOS is free software: You can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GLADOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with GLADOS. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Date: 18 August 2016
 * Authors: Jan Stephan <j.stephan@hzdr.de>
 */

#ifndef GLADOS_CUFFT_PLAN_H_
#define GLADOS_CUFFT_PLAN_H_

#include <cstddef>
#include <type_traits>
#include <utility>

#include <cufft.h>

#include <glados/cufft/exception.h>

namespace glados
{
    namespace cufft
    {
        namespace detail
        {
            template <class I, class O> struct type_chooser {};
            template <> struct type_chooser<cufftReal, cufftComplex> { static constexpr auto value = CUFFT_R2C; };
            template <> struct type_chooser<cufftComplex, cufftReal> { static constexpr auto value = CUFFT_C2R; };
            template <> struct type_chooser<cufftComplex, cufftComplex> { static constexpr auto value = CUFFT_C2C; };
            template <> struct type_chooser<cufftDoubleReal, cufftDoubleComplex> { static constexpr auto value = CUFFT_D2Z; };
            template <> struct type_chooser<cufftDoubleComplex, cufftDoubleReal> { static constexpr auto value = CUFFT_Z2D; };
            template <> struct type_chooser<cufftDoubleComplex, cufftDoubleComplex> { static constexpr auto value = CUFFT_Z2Z; };

            template <class I> struct type_mapper {};
            template <> struct type_mapper<cufftReal> { using type = cufftComplex; };
            template <> struct type_mapper<cufftComplex> { using type = cufftReal; };
            template <> struct type_mapper<cufftDoubleReal> { using type = cufftDoubleComplex; };
            template <> struct type_mapper<cufftDoubleComplex> { using type = cufftDoubleReal; };
        }

        template <cufftType type>
        class plan
        {
            public:
                static constexpr auto transformation_type = type;

                plan() noexcept = default;
                plan(int nx) : valid_{true} { handle_result(cufftPlan1d(&handle_, nx, transformation_type, 1)); }
                plan(int nx, int ny) : valid_{true} { handle_result(cufftPlan2d(&handle_, nx, ny, transformation_type)); }
                plan(int nx, int ny, int nz) : valid_{true} { handle_result(cufftPlan3d(&handle_, nx, ny, nz, transformation_type)); }

                plan(int rank, int* n, int* inembed, int istride, int idist,
                                       int* onembed, int ostride, int odist,
                                       int batch)
                : valid_{true}
                {
                    handle_result(cufftPlanMany(&handle_, rank, n, inembed, istride, idist, onembed, ostride, odist, transformation_type, batch));
                }

                plan(const plan& other) noexcept
                : handle_{other.handle_}, valid_{other.valid_}
                {}

                auto operator=(const plan& other) noexcept -> plan&
                {
                    handle_ = other.handle_;
                    valid_ = other.valid_;
                    return *this;
                }


                plan(plan&& other) noexcept
                : handle_{std::move(other.handle_)}, valid_{other.valid_}
                {
                    other.valid_ = false;
                }

                auto operator=(plan&& other) noexcept -> plan&
                {
                    handle_ = std::move(other.handle_);
                    valid_ = other.valid_;
                    other.valid_ = false;
                    return *this;
                }

                ~plan() { if(valid_) cufftDestroy(handle_); }

                auto get() noexcept -> cufftHandle
                {
                    return handle_;
                }

                template <class I, class O>
                auto execute(I* idata, O* odata) -> void
                {
                    static_assert(!std::is_same<I, O>::value, "Plan needs a direction for transformations between the same types.");
                    static_assert(detail::type_chooser<I, O>::value == transformation_type, "This plan can not be used for other types than originally specified.");
                    static_assert(std::is_same<O, typename detail::type_mapper<I>::type>::value, "Attempt to transform to an incompatible type.");

                    cufft_exec(idata, odata);
                }

                template <class I, class O>
                auto execute(I* idata, O* odata, int direction) -> void
                {
                    static_assert(std::is_same<I, O>::value, "Transformations between different types are implicitly inverse");
                    static_assert(detail::type_chooser<I, O>::value == transformation_type, "This plan can not be used for other types than originally specified.");
                    static_assert(std::is_same<O, typename detail::type_mapper<I>::type>::value, "Attempt to transform to an incompatible type.");

                    cufft_exec(idata, odata, direction);
                }

                auto set_stream(cudaStream_t stream) -> void
                {
                    handle_result(cufftSetStream(handle_, stream));
                }

            private:
                auto handle_result(cufftResult res) const -> void
                {
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wswitch-enum"
                    switch(res)
                    {
                        case CUFFT_SUCCESS:         break;
                        case CUFFT_INVALID_PLAN:    throw invalid_argument{"The plan parameter is not a valid handle."};
                        case CUFFT_ALLOC_FAILED:    throw bad_alloc{};
                        case CUFFT_INVALID_VALUE:   throw invalid_argument{"One or more invalid parameters were passed to the API."};
                        case CUFFT_INTERNAL_ERROR:  throw runtime_error{"An internal driver error was detected."};
                        case CUFFT_EXEC_FAILED:     throw runtime_error{"cuFFT failed to execute the transform on the GPU."};
                        case CUFFT_SETUP_FAILED:    throw runtime_error{"The cuFFT library failed to initialize."};
                        case CUFFT_INVALID_SIZE:    throw invalid_argument{"One or more of the parameters is not a supported size."};
                        default:                    throw runtime_error{"Unknown error."};
                    }
                    #pragma GCC diagnostic pop
                }

                auto cufft_exec(cufftComplex* idata, cufftComplex* odata, int direction) -> void
                {
                    handle_result(cufftExecC2C(handle_, idata, odata, direction));
                }

                auto cufft_exec(cufftDoubleComplex* idata, cufftDoubleComplex* odata, int direction) -> void
                {
                    handle_result(cufftExecZ2Z(handle_, idata, odata, direction));
                }

                auto cufft_exec(cufftReal* idata, cufftComplex* odata) -> void
                {
                    handle_result(cufftExecR2C(handle_, idata, odata));
                }

                auto cufft_exec(cufftDoubleReal* idata, cufftDoubleComplex* odata) -> void
                {
                    handle_result(cufftExecD2Z(handle_, idata, odata));
                }

                auto cufft_exec(cufftComplex* idata, cufftReal* odata) -> void
                {
                    handle_result(cufftExecC2R(handle_, idata, odata));
                }

                auto cufft_exec(cufftDoubleComplex* idata, cufftDoubleReal* odata) -> void
                {
                    handle_result(cufftExecZ2D(handle_, idata, odata));
                }

            private:
                bool valid_ = false;
                cufftHandle handle_ = 0;
        };
    }
}

#endif /* GLADOS_CUFFT_PLAN_H_ */
