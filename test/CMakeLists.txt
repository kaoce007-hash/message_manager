set(test_output ${CMAKE_CURRENT_BINARY_DIR}/output)
file(MAKE_DIRECTORY ${test_output})
foreach(name indexed_set_tests direct_alignment_tests legacy_tests tests alignment_tests msgcode_tests interface_v6_tests interface_v7_tests field_mapping_tests bitfield_tests integer_format_tests simple_structure_tests)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE binary_message)
endforeach()
add_test(NAME legacy_binary_tests COMMAND legacy_tests)
add_test(NAME message_tests COMMAND tests ${CMAKE_CURRENT_SOURCE_DIR}/data/json)
add_test(NAME alignment_tests COMMAND alignment_tests ${CMAKE_CURRENT_SOURCE_DIR}/data/json)
add_test(NAME msgcode_tests COMMAND msgcode_tests ${CMAKE_CURRENT_SOURCE_DIR}/data/v5/json)
add_test(NAME interface_v6_tests COMMAND interface_v6_tests ${CMAKE_CURRENT_SOURCE_DIR}/data/v6/json)
add_test(NAME interface_v7_tests COMMAND interface_v7_tests ${CMAKE_CURRENT_SOURCE_DIR}/data/current/json)
add_test(NAME field_mapping_tests COMMAND field_mapping_tests ${PROJECT_SOURCE_DIR}/json)

if(UNIX)
    add_executable(audit_integration_tests audit_integration_tests.cpp)
    target_link_libraries(audit_integration_tests PRIVATE binary_message)
    add_test(NAME audit_integration_tests COMMAND audit_integration_tests ${CMAKE_CURRENT_SOURCE_DIR}/data/current/json)
    set_tests_properties(audit_integration_tests PROPERTIES
        WORKING_DIRECTORY ${test_output} FIXTURES_SETUP AuditWireCases TIMEOUT 60)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME audit_wire_oracle COMMAND ${Python3_EXECUTABLE} -I
            ${CMAKE_CURRENT_SOURCE_DIR}/audit_wire_oracle.py ${test_output}/audit_output/oracle_cases.json)
        set_tests_properties(audit_wire_oracle PROPERTIES FIXTURES_REQUIRED AuditWireCases)
    endif()
endif()

add_test(NAME simple_structure_tests COMMAND simple_structure_tests ${PROJECT_SOURCE_DIR}/json/integer_formats)

add_test(NAME integer_format_tests COMMAND integer_format_tests ${PROJECT_SOURCE_DIR}/json/integer_formats)

add_test(NAME bitfield_tests COMMAND bitfield_tests ${PROJECT_SOURCE_DIR}/json/bitfields)

add_test(NAME direct_alignment_tests COMMAND direct_alignment_tests)

add_test(NAME indexed_set_tests COMMAND indexed_set_tests)
