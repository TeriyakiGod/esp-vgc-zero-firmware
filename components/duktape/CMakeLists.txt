idf_component_register(SRCS "duktape.c" INCLUDE_DIRS ".")

# Disable warnings as errors for this component
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
