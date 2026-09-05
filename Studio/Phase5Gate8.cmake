# Dedicated Gate 8 Studio source. Keep baking UI outside StudioApplication.cpp;
# that file receives only bounded lifecycle hooks through guarded integration.
target_sources(RenegadeStudio PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/Phase5Gate8LightmapBaking.cpp
)
