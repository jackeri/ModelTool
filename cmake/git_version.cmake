# Version from the latest git tag named v<major>.<minor>[.<patch>][suffix], like v1.2.3 or v1.3.0-rc1:
#   MODELTOOL_VERSION       numeric version for project(), installers and bundles: 1.2.3
#   MODELTOOL_VERSION_FULL  1.2.3 on the tagged commit, 1.2.3-<commits after the tag>-g<hash> after it
# Without git or a tag the version is 0.0.0.

set(MODELTOOL_VERSION "0.0.0")
set(MODELTOOL_VERSION_FULL "0.0.0-unknown")

find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --long --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE git_describe
            RESULT_VARIABLE git_result
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)

    if (git_result EQUAL 0 AND git_describe MATCHES "^v([0-9]+)\\.([0-9]+)(\\.([0-9]+))?(.*)-([0-9]+)-g([0-9a-f]+)$")
        set(version_major "${CMAKE_MATCH_1}")
        set(version_minor "${CMAKE_MATCH_2}")
        set(version_patch "${CMAKE_MATCH_4}")
        set(version_suffix "${CMAKE_MATCH_5}")
        set(version_commits "${CMAKE_MATCH_6}")
        set(version_hash "${CMAKE_MATCH_7}")
        if (version_patch STREQUAL "")
            set(version_patch 0)
        endif ()

        set(MODELTOOL_VERSION "${version_major}.${version_minor}.${version_patch}")
        set(MODELTOOL_VERSION_FULL "${MODELTOOL_VERSION}${version_suffix}")
        if (NOT version_commits EQUAL 0)
            set(MODELTOOL_VERSION_FULL "${MODELTOOL_VERSION_FULL}-${version_commits}-g${version_hash}")
        endif ()
    else ()
        message(STATUS "No v<version> git tag found, the version is ${MODELTOOL_VERSION}")
    endif ()

    # Configure again when the commit or the tags change
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --absolute-git-dir
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE git_dir
            RESULT_VARIABLE git_result
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    if (git_result EQUAL 0)
        foreach (git_file "HEAD" "packed-refs" "refs/tags")
            if (EXISTS "${git_dir}/${git_file}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${git_dir}/${git_file}")
            endif ()
        endforeach ()
    endif ()
endif ()

message(STATUS "ModelTool version ${MODELTOOL_VERSION_FULL}")
