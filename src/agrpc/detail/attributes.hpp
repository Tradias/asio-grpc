#ifndef AGRPC_DETAIL_ATTRIBUTES_HPP
#define AGRPC_DETAIL_ATTRIBUTES_HPP

#include <version>

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(unlikely)
#define AGRPC_UNLIKELY [[unlikely]]
#endif
#endif
#ifndef AGRPC_UNLIKELY
#define AGRPC_UNLIKELY
#endif

#endif  // AGRPC_DETAIL_ATTRIBUTES_HPP
