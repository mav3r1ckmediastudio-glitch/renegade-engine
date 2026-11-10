#include "Probe.h"
template<size_t... I> Addresses addressesB(std::index_sequence<I...>) {
 return {static_cast<void*>(wi::allocator::shared_block_allocator<Probe<I>>) ...};
}
Addresses AddressesB() { return addressesB(std::make_index_sequence<100>{}); }
void ReleaseB(wi::allocator::shared_ptr<Probe<0>>& p) { p.reset(); }
