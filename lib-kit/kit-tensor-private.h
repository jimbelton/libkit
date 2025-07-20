#ifndef KIT_TENSOR_PRIVATE_H
#define KIT_TENSOR_PRIVATE_H

#if defined(SXE_DEBUG) || defined(SXE_COVERAGE)    // Define unique tags for mockfails
#   define KIT_TENSOR_MAKE_BEGIN ((const char *)kit_tensor_init + 0)
#   define KIT_TENSOR_MATMUL ((const char *)kit_tensor_init + 1)
#endif

#endif
