# Helper invoked via `cmake -P` from Packaging.cmake's custom commands:
# copies every file in SRC_DIR matching PATTERN (a glob, e.g. "*.dll") into
# DST_DIR. Exists only because `cmake -E copy_if_different` doesn't accept
# wildcards, and `-E copy_directory` would copy everything (.pdb, .lib,
# ...) rather than just the redistributable DLLs.
file(GLOB APO_MATCHED_FILES "${SRC_DIR}/${PATTERN}")
foreach(f ${APO_MATCHED_FILES})
    file(COPY "${f}" DESTINATION "${DST_DIR}")
endforeach()
