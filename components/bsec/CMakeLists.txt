# BSEC Component
idf_component_register(
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
    REQUIRES driver
)

# Link the precompiled BSEC library
target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/lib/libalgobsec.a")


