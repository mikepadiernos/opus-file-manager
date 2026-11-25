corrosion_import_crate(
    MANIFEST_PATH Cargo.toml
    CRATES opus_searchquery
    CRATE_TYPES staticlib
)
target_include_directories(opus_searchquery INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
