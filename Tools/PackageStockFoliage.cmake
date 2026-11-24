# Synchronize owned stock foliage without touching project-owned palettes or mixes.
if(NOT DEFINED FOLIAGE_SOURCE OR NOT DEFINED FOLIAGE_DESTINATION)
    message(FATAL_ERROR "FOLIAGE_SOURCE and FOLIAGE_DESTINATION are required")
endif()
set(retired_stock_plants
    "01_short_grass_albedo.png"
    "02_long_grass_albedo.png"
    "03_dead_grass_albedo.png"
    "04_burnt_grass_albedo.png"
    "05_wheat_albedo.png"
    "06_daisies_albedo.png"
    "07_poppies_albedo.png"
    "08_nettles_albedo.png"
    "09_clover_albedo.png"
    "10_thistles_albedo.png"
    "Alien/01_lumen_grass_albedo.png"
    "Alien/02_crimson_razorgrass_albedo.png"
    "Alien/03_spore_grass_albedo.png"
    "Alien/04_crystal_grass_albedo.png"
    "Alien/05_velvet_frond_albedo.png"
    "Alien/06_glowcap_grass_albedo.png"
    "Alien/07_void_leaf_grass_albedo.png"
    "Alien/08_curlgrass_albedo.png"
    "Alien/09_coral_grass_albedo.png"
    "Alien/10_rift_blossom_albedo.png"
    "Freshwater/06_vallisneria_albedo.png"
    "Freshwater/07_amazon_sword_albedo.png"
    "Freshwater/08_anubias_albedo.png"
    "Freshwater/09_java_fern_albedo.png"
    "Freshwater/10_red_ludwigia_albedo.png"
    "Marine/01_coastal_seagrass_albedo.png"
    "Marine/02_sea_lettuce_albedo.png"
    "Marine/03_red_algae_albedo.png"
    "Marine/04_feather_algae_albedo.png"
    "Marine/05_golden_kelp_albedo.png"
)
foreach(plant IN LISTS retired_stock_plants)
    file(REMOVE "${FOLIAGE_DESTINATION}/${plant}")
endforeach()
file(MAKE_DIRECTORY "${FOLIAGE_DESTINATION}/Presets")
file(COPY "${FOLIAGE_SOURCE}/Presets/meadow_open.png"
          "${FOLIAGE_SOURCE}/Presets/meadow_full.png"
          "${FOLIAGE_SOURCE}/Presets/short_grass.png"
          "${FOLIAGE_SOURCE}/Presets/dry_grass.png"
          "${FOLIAGE_SOURCE}/Presets/seedhead_grass.png"
          "${FOLIAGE_SOURCE}/Presets/low_clover.png"
          "${FOLIAGE_SOURCE}/Presets/small_wildflowers.png"
     DESTINATION "${FOLIAGE_DESTINATION}/Presets")
