find_package(Doxygen)

function(asio_grpc_doxygen)
    if(NOT TARGET Doxygen::doxygen)
        return()
    endif()

    set(DOXYGEN_PROJECT_NUMBER "v${PROJECT_VERSION}")
    set(DOXYGEN_OUTPUT_DIRECTORY "${ASIO_GRPC_PROJECT_ROOT}")
    set(DOXYGEN_SHOW_USED_FILES "NO")
    set(DOXYGEN_SHOW_FILES "NO")
    set(DOXYGEN_CASE_SENSE_NAMES "NO")
    set(DOXYGEN_PYTHON_DOCSTRING "NO")
    set(DOXYGEN_BUILTIN_STL_SUPPORT "YES")
    set(DOXYGEN_IDL_PROPERTY_SUPPORT "NO")
    set(DOXYGEN_EXTRACT_PACKAGE "YES")
    set(DOXYGEN_EXTRACT_STATIC "YES")
    set(DOXYGEN_STRIP_FROM_INC_PATH "src")
    set(DOXYGEN_HIDE_UNDOC_MEMBERS "YES")
    set(DOXYGEN_HIDE_UNDOC_CLASSES "YES")
    set(DOXYGEN_WARN_IF_UNDOCUMENTED "NO")
    set(DOXYGEN_WARN_IF_INCOMPLETE_DOC "NO")
    set(DOXYGEN_FILE_PATTERNS "*.cpp" "*.hpp" "*.md" "cmake/AsioGrpcProtobufGenerator.cmake")
    set(DOXYGEN_EXCLUDE_PATTERNS "*/build*" "*/example/*" "*/deps/*" "*/test/*" "README.md" "CONTRIBUTING.md")
    set(DOXYGEN_EXAMPLE_PATH "example/proto" "example/snippets" "example" "test/cmake/superbuild/src" "cmake")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "doc/index.md")
    set(DOXYGEN_VERBATIM_HEADERS "NO")
    set(DOXYGEN_HTML_OUTPUT "docs")
    set(DOXYGEN_INLINE_INHERITED_MEMB "YES")
    set(DOXYGEN_SORT_MEMBER_DOCS "NO")

    # `doxygen -w html header.html delete_me.html delete_me.css`
    set(DOXYGEN_HTML_HEADER "${CMAKE_CURRENT_LIST_DIR}/header.html")

    set(DOXYGEN_DISABLE_INDEX "NO")
    set(DOXYGEN_FULL_SIDEBAR "NO")
    set(DOXYGEN_HTML_EXTRA_FILES "${CMAKE_CURRENT_LIST_DIR}/doxygen-awesome-darkmode-toggle.js")
    set(DOXYGEN_HTML_EXTRA_STYLESHEET
        "${CMAKE_CURRENT_LIST_DIR}/doxygen-awesome.css" "${CMAKE_CURRENT_LIST_DIR}/doxygen-awesome-sidebar-only.css"
        "${CMAKE_CURRENT_LIST_DIR}/doxygen-awesome-sidebar-only-darkmode-toggle.css"
        "${CMAKE_CURRENT_LIST_DIR}/custom.css")
    set(DOXYGEN_HTML_COLORSTYLE "LIGHT")
    set(DOXYGEN_HTML_COLORSTYLE_HUE "209")
    set(DOXYGEN_HTML_COLORSTYLE_SAT "255")
    set(DOXYGEN_HTML_COLORSTYLE_GAMMA "113")
    set(DOXYGEN_HTML_DYNAMIC_MENUS "YES")
    set(DOXYGEN_HTML_DYNAMIC_SECTIONS "YES")
    set(DOXYGEN_GENERATE_TREEVIEW "YES")
    set(DOXYGEN_TREEVIEW_WIDTH "335")
    set(DOXYGEN_MACRO_EXPANSION "YES")
    set(DOXYGEN_INCLUDE_PATH "${ASIO_GRPC_PROJECT_ROOT}/src" "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    set(DOXYGEN_PREDEFINED
        "AGRPC_BOOST_ASIO" "AGRPC_GENERATING_DOCUMENTATION" "AGRPC_ASIO_HAS_CO_AWAIT"
        "AGRPC_ASIO_HAS_CANCELLATION_SLOT" "AGRPC_ASIO_HAS_NEW_SPAWN" "AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR")
    set(DOXYGEN_SKIP_FUNCTION_MACROS "NO")

    doxygen_add_docs(asio-grpc-doxygen WORKING_DIRECTORY "${ASIO_GRPC_PROJECT_ROOT}")
endfunction()

asio_grpc_doxygen()
