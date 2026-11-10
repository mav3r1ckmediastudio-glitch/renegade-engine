#include "Probe.h"
int main() {
 auto a=AddressesA(), b=AddressesB(), c=AddressesC();
 unsigned count=wi::allocator::get_shared_allocator_count();
 size_t mismatches=0;
 for(size_t i=0;i<a.size();++i) if(a[i]!=b[i] || a[i]!=c[i]) ++mismatches;
 std::cout<<"registered="<<count<<" expected=100 mismatched_types="<<mismatches<<std::endl;
 if(count!=100 || mismatches) return 1;
 auto first=MakeA(); auto second=first;
 if(first->value!=42 || first.use_count()!=2) return 2;
 ReleaseB(first);
 if(second.use_count()!=1 || destroyed!=0) return 3;
 ReleaseB(second);
 if(destroyed!=1) return 4;
 std::cout<<"Cross-translation-unit ownership and destruction PASS"<<std::endl;
}
