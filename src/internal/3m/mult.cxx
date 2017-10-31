#include "mult.hpp"

#include "util/gemm_thread.hpp"

#include "nodes/gemm.hpp"

namespace tblis
{

MemoryPool BuffersForA(4096);
MemoryPool BuffersForB(4096);

namespace internal
{

template <typename T>
void mult(const communicator& comm, const config& cfg,
          len_type m, len_type n, len_type k,
          T alpha, bool conj_A, const T* A, stride_type rs_A, stride_type cs_A,
                   bool conj_B, const T* B, stride_type rs_B, stride_type cs_B,
          T  beta, bool conj_C,       T* C, stride_type rs_C, stride_type cs_C)
{
    TBLIS_ASSERT(!conj_A && !conj_B && !conj_C);

    const bool row_major = cfg.gemm_row_major.value<T>();

    if ((row_major ? rs_C : cs_C) == 1)
    {
        /*
         * Compute C^T = B^T * A^T instead
         */
        std::swap(m, n);
        std::swap(A, B);
        std::swap(rs_A, cs_B);
        std::swap(rs_B, cs_A);
        std::swap(rs_C, cs_C);
    }

    matrix_view<T> Av({m, k}, const_cast<T*>(A), {rs_A, cs_A});
    matrix_view<T> Bv({k, n}, const_cast<T*>(B), {rs_B, cs_B});
    matrix_view<T> Cv({m, n},                C , {rs_C, cs_C});

    int nt = comm.num_threads();
    gemm_thread_config tc = make_gemm_thread_config<T>(cfg, nt, m, n, k);

    GotoGEMM gemm;
    step<0>(gemm).subcomm =                  comm.gang(TCI_EVENLY, tc.jc_nt);
    step<3>(gemm).subcomm = step<0>(gemm).subcomm.gang(TCI_EVENLY, tc.ic_nt);
    step<5>(gemm).subcomm = step<3>(gemm).subcomm.gang(TCI_EVENLY, tc.jr_nt);
    step<6>(gemm).subcomm = step<5>(gemm).subcomm.gang(TCI_EVENLY, tc.ir_nt);

    gemm(comm, cfg, alpha, Av, Bv, beta, Cv);

    comm.barrier();
}

#define FOREACH_TYPE(T) \
template void mult(const communicator& comm, const config& cfg, \
                   len_type m, len_type n, len_type k, \
                   T alpha, bool conj_A, const T* A, stride_type rs_A, stride_type cs_A, \
                            bool conj_B, const T* B, stride_type rs_B, stride_type cs_B, \
                   T  beta, bool conj_C,       T* C, stride_type rs_C, stride_type cs_C);
#include "configs/foreach_type.h"

}
}
