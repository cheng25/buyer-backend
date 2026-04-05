cmake_minimum_required(VERSION 3.14)
project(Boost CXX)

add_library(Boost::assert INTERFACE IMPORTED GLOBAL)
target_include_directories(Boost::assert INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/assert/include)

add_library(Boost::config INTERFACE IMPORTED GLOBAL)
target_include_directories(Boost::config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/config/include)

add_library(Boost::type_traits INTERFACE IMPORTED GLOBAL)
target_include_directories(Boost::type_traits INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/type_traits/include)

add_library(Boost::throw_exception INTERFACE IMPORTED GLOBAL)
target_include_directories(Boost::throw_exception INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/throw_exception/include)

add_library(Boost::core INTERFACE IMPORTED GLOBAL)
target_include_directories(Boost::core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/core/include)

add_library(Boost::uuid INTERFACE IMPORTED GLOBAL)
target_include_directories(Boost::uuid INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/uuid/include)

add_library(Boost::boost INTERFACE IMPORTED GLOBAL)
target_link_libraries(Boost::boost INTERFACE
    Boost::assert
    Boost::config
    Boost::type_traits
    Boost::throw_exception
    Boost::core
    Boost::uuid
)