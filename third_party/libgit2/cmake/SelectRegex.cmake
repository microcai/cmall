add_feature_info(regex ON "using bundled PCRE")
set(GIT_REGEX_BUILTIN 1)

add_subdirectory("${PROJECT_SOURCE_DIR}/deps/pcre" "${PROJECT_BINARY_DIR}/deps/pcre")
list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${PROJECT_SOURCE_DIR}/deps/pcre")
list(APPEND LIBGIT2_DEPENDENCY_OBJECTS $<TARGET_OBJECTS:pcre>)
