#include "Probe.h"
template<size_t... I> Addresses addressesC(std::index_sequence<I...>) {
 return {static_cast<void*>(wi::allocator::shared_block_allocator<Probe<I>>) ...};
}
Addresses AddressesC() { return addressesC(std::make_index_sequence<100>{}); }
