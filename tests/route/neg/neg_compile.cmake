# Builds TARGET in BUILD_DIR. Fails (so that the WILL_FAIL test passes) only when the build
# fails and its output contains EXPECT; succeeds, and so fails the test, when the target
# compiles or fails for another reason.
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${TARGET}" --config "${CONFIG}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(rc EQUAL 0)
    message("${TARGET} compiled, and it must not")
    return()
endif()
string(FIND "${out}" "${EXPECT}" at)
if(at EQUAL -1)
    message("${TARGET} failed to compile, but its diagnostic does not contain '${EXPECT}':\n${out}")
    return()
endif()
message(FATAL_ERROR "${TARGET} was rejected as expected: '${EXPECT}'")
