#
# find the Kvrange library and set up an imported target for it
#

#
# inputs:
#   - KVRANGE_INCLUDE_DIR: hint for finding kvrange/db.h and others
#   - KVRANGE_LIBRARY_DIR: hint for finding the Kvrange lib
#
# output:
#   - "kvrange" library target
#   - KVRANGE_FOUND  (set if found)
#

include (FindPackageHandleStandardArgs)

find_path (KVRANGE_INCLUDE kvrange/db.h HINTS ${KVRANGE_INCLUDE_DIR})
find_library (KVRANGE_LIBRARY kvrangedb HINTS ${KVRANGE_LIBRARY})

find_package_handle_standard_args (Kvrange DEFAULT_MSG
        KVRANGE_INCLUDE KVRANGE_LIBRARY)

mark_as_advanced (KVRANGE_INCLUDE KVRANGE_LIBRARY)

if (KVRANGE_FOUND AND NOT TARGET kvrange)
    add_library (kvrange UNKNOWN IMPORTED)
    if (NOT "${KVRANGE_INCLUDE}" STREQUAL "/usr/include")
        set_target_properties (kvrange PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${KVRANGE_INCLUDE}")
    endif ()
    set_property (TARGET kvrange APPEND PROPERTY
            IMPORTED_LOCATION "${KVRANGE_LIBRARY}")
endif ()
