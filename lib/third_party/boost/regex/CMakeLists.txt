project(boost-regex)

add_library(boost-regex INTERFACE)

target_include_directories(boost-regex INTERFACE
    include
)
target_compile_definitions(boost-regex INTERFACE BOOST_REGEX_STANDALONE)

add_library(boost::regex ALIAS boost-regex)