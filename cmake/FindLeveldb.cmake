#
# find the Leveldb library and set up an imported target for it
#

#
# inputs:
#   - LEVELDB_INCLUDE_DIR: hint for finding leveldb/db.h and others
#   - LEVELDB_LIBRARY_DIR: hint for finding the Leveldb lib
#
# output:
#   - "leveldb" library target
#   - LEVELDB_FOUND  (set if found)
#

include (FindPackageHandleStandardArgs)

find_path (LEVELDB_INCLUDE leveldb/db.h HINTS ${LEVELDB_INCLUDE_DIR})
find_library (LEVELDB_LIBRARY leveldb HINTS ${LEVELDB_LIBRARY_DIR})

find_package_handle_standard_args (Leveldb DEFAULT_MSG
        LEVELDB_INCLUDE LEVELDB_LIBRARY)

mark_as_advanced (LEVELDB_INCLUDE LEVELDB_LIBRARY)

if (LEVELDB_FOUND AND NOT TARGET leveldb)
    add_library (leveldb UNKNOWN IMPORTED)
    if (NOT "${LEVELDB_INCLUDE}" STREQUAL "/usr/include")
        set_target_properties (leveldb PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${LEVELDB_INCLUDE}")
    endif ()
    set_property (TARGET leveldb APPEND PROPERTY
            IMPORTED_LOCATION "${LEVELDB_LIBRARY}")
endif ()
