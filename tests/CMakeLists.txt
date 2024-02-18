if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   add_executable(demo "src/demo.cpp")
   target_link_libraries(demo regexmatcher_core)
endif()

add_executable(matcher-tests "src/main.cpp")
target_link_libraries(matcher-tests regexmatcher_core)

add_test(NAME matcher-urls
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "/" "\\/([0-9a-z-]+)"
         --match "/" "/12asdf-"
)

add_test(NAME matcher-test-no-regex
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text1" "text2" "text3"
         --match "text" "text1" "text2" "text3" "text4"
)

add_test(NAME matcher-test-group
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text(1)" "text(2)" "text(3)"
         --match "text" "text1" "text2" "text3" "text4"
)

add_test(NAME matcher-test-global-or
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text1|test2|test3"
         --match "text" "text1" "text2" "text3" "text4"
)

add_test(NAME matcher-test-or-within-group
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text(1|2|3)"
         --match "text" "text1" "text2" "text3" "text4"
)

add_test(NAME matcher-test-letter-asterix
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text1*"
         --match "text" "text1" "text2" "text3" "text4"
)

add_test(NAME matcher-test-group-asterix
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text(1)*"
         --match "text" "text1" "text2" "text3" "text4"
)

add_test(NAME matcher-test-group-with-or-asterix
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text(1|2|3)*"
         --match "text" "text1" "text2" "text3" "text4"
)

add_test(NAME matcher-test-group-with-or-plus
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text(1|2|3)+"
         --match "text" "text1" "text21" "text31" "text34"
)

add_test(NAME matcher-test-group-with-repeat
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text(1|2|3){1,2}"
         --match "text" "text1" "text21" "text31" "text34" "text123123"
)

add_test(NAME matcher-test-group-with-question-mark
         COMMAND $<TARGET_FILE:matcher-tests>
         --add "text(1|2|3)?"
         --match "text" "text1" "text21" "text31" "text34" "text123123"
)


add_test(NAME matcher-tests-many-regexes-many-matches
         COMMAND $<TARGET_FILE:matcher-tests> 
         --add
            "d(abc|def)*g+"
            "d(abc)*g+"
            "a?"
            "b|c"
            "(d|e)f"
            "f[a-c]?d(ab|cd)*g+"
            "a{1,3}a"
            "aaa"
            "aa"
         --match
            "a"
            "aa"
            "aaa"
            "aaaa"
            "a"
            "b"
            "c"
            "d"
            "df"
            "e"
            "ef"
            "fdg"
            "fdgg"
            "fd"
            "fdabgg"
            "fdababgg"
            "fdabcdgg"
            "fdcdcdgg"
            "fdacgg"
            "fadabgg"
            "fadababgg"
            "fadabcdgg"
            "fadcdcdgg"
            "fadacgg"
            "fbdabgg"
            "fbdababgg"
            "fbdabcdgg"
            "fbdcdcdgg"
            "fbdacgg"
            "fcdabgg"
            "fcdababgg"
            "fcdabcdgg"
            "fcdcdcdgg"
            "fcdacgg"
            "fccdabgg"
            "fccdababgg"
            "fccdabcdgg"
            "fccdcdcdgg"
            "fccdacgg"
            "dab"
            "dabcabc"
            "dabc"
            "ddefdef"
            "dabcg"
            "dg"
            "dabcabcg"
            "ddefabcg"
            "ddefdefg"
)

add_custom_target(tests COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_BUILD_TYPE} --output-on-failure
                  DEPENDS matcher-tests)
