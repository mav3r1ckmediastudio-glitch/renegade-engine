#pragma once
#include <functional>
#include <cstddef>
namespace renegade::bridge {
using TerrainOperationProgress = std::function<void(const char*,std::size_t,std::size_t)>;
inline thread_local const TerrainOperationProgress* terrainOperationProgress = nullptr;
class ScopedTerrainOperationProgress {
    const TerrainOperationProgress* previous_;
public:
    explicit ScopedTerrainOperationProgress(const TerrainOperationProgress& progress)
        :previous_(terrainOperationProgress){terrainOperationProgress=&progress;}
    ~ScopedTerrainOperationProgress(){terrainOperationProgress=previous_;}
    ScopedTerrainOperationProgress(const ScopedTerrainOperationProgress&)=delete;
    ScopedTerrainOperationProgress& operator=(const ScopedTerrainOperationProgress&)=delete;
};
inline void ReportTerrainOperation(const char* stage,std::size_t completed=0,std::size_t total=0) noexcept {
    // Telemetry must never alter transaction success or rollback.
    if(terrainOperationProgress)try{(*terrainOperationProgress)(stage,completed,total);}catch(...){}
}
}
