add_library(opusfm_mcp STATIC
    mcpserver.cpp
    mcpserver.h
)
target_include_directories(opusfm_mcp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(opusfm_mcp PUBLIC Qt6::Core)

# MCP clients start the server and talk to it over stdin and stdout.
add_executable(opus-file-manager-mcp main.cpp)
target_include_directories(opus-file-manager-mcp PRIVATE ${PROJECT_BINARY_DIR}/src)
target_link_libraries(opus-file-manager-mcp PRIVATE opusfm_mcp opusfm_core)

install(TARGETS opus-file-manager-mcp ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
