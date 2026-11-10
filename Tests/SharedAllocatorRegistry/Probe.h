#pragma once
#include "wiAllocator.h"
#include <array>
#include <utility>
#include <iostream>
inline int destroyed = 0;
template<size_t I> struct Probe { int value = 42; ~Probe() { ++destroyed; } };
using Addresses = std::array<void*,100>;
Addresses AddressesA(); Addresses AddressesB(); Addresses AddressesC();
wi::allocator::shared_ptr<Probe<0>> MakeA();
void ReleaseB(wi::allocator::shared_ptr<Probe<0>>&);
