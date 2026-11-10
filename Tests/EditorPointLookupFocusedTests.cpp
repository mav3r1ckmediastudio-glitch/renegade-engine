#include "EditorPointLookupWorkProbe.h"
#include <iostream>
int main(){bool ok=renegade::tests::EditorPointLookupAvoidsSceneEnumeration();std::cout<<"EditorPointLookupWorkProbe "<<(ok?"PASS":"FAIL")<<std::endl;return ok?0:1;}
