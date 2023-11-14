cmake_minimum_required(VERSION 3.16.3)

project(RegexMatcher)

# add_library(Node STATIC src/node.cpp
#                         src/symbol.cpp
#                         src/limits.cpp
#                         src/RegexMatcher.cpp
# )
# target_include_directories(Node PUBLIC lib/)
# set_property(TARGET Node PROPERTY CXX_STANDARD 17)

add_executable(Tests tests/main.cpp)
target_include_directories(Tests PUBLIC lib/)
set_property(TARGET Tests PROPERTY CXX_STANDARD 17)
# target_link_libraries(Tests PUBLIC Node)
