# Runs Doxygen on one Doxyfile and stops on any warning it prints:
#
#   cmake -DWXL_DOXYGEN=<doxygen> -DWXL_DOXYFILE=<Doxyfile> -P wxl_doxygen_run.cmake
#
# WARN_AS_ERROR in the Doxyfile fails the run on what Doxygen writes to its
# warning log. What it says about the configuration itself goes to the console
# and leaves the exit code alone: an OUTPUT_LANGUAGE it does not know is
# replaced with English, said in a warning, and the run succeeds. A site target
# and its test run Doxygen through here (wxl_doxygen.cmake), so that warning
# stops them as well. The output is shown as it comes.

execute_process(
    COMMAND "${WXL_DOXYGEN}" "${WXL_DOXYFILE}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _said
    ERROR_VARIABLE _said
    ECHO_OUTPUT_VARIABLE
    ECHO_ERROR_VARIABLE)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Doxygen failed on ${WXL_DOXYFILE}: ${_result}")
endif()
string(TOLOWER "${_said}" _said)
if(_said MATCHES "warning")
    message(FATAL_ERROR "Doxygen printed a warning on ${WXL_DOXYFILE}; the site is built without one or not at all.")
endif()
