#include "Probe.h"
template<size_t... I> Addresses addressesA(std::index_sequence<I...>) {
 return {static_cast<void*>(wi::allocator::shared_block_allocator<Probe<I>>) ...};
}
Addresses AddressesA() { return addressesA(std::make_index_sequence<100>{}); }
wi::allocator::shared_ptr<Probe<0>> MakeA() { return wi::allocator::make_shared<Probe<0>>(); }
