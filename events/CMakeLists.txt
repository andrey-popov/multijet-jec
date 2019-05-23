cmake_minimum_required(VERSION 3.11 FATAL_ERROR)
project(multijet CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -pedantic)


set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")


# External dependencies
find_package(Boost 1.63 COMPONENTS filesystem program_options REQUIRED)
find_package(ROOT 6 REQUIRED)
find_package(mensura REQUIRED)


# Main library and ROOT dictionary needed to read input files
include("${ROOT_USE_FILE}/../modules/RootNewMacros.cmake")
set(CMAKE_INSTALL_LIBDIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
root_generate_dictionary(multijet-plugins_dict
    src/classes.h LINKDEF src/classes_def.xml
    MODULE multijet-plugins
    OPTIONS "-I${CMAKE_SOURCE_DIR}/include"
)
add_library(multijet-plugins SHARED
    src/AngularFilter.cpp
    src/BalanceCalc.cpp
    src/BalanceFilter.cpp
    src/BalanceHists.cpp
    src/BalanceVars.cpp
    src/BasicJetVars.cpp
    src/DumpEventID.cpp
    src/DumpWeights.cpp
    src/EtaPhiFilter.cpp
    src/FirstJetFilter.cpp
    src/GenMatchFilter.cpp
    src/JERCJetMETReader.cpp
    src/JERCJetMETUpdate.cpp
    src/JetIDFilter.cpp
    src/LeadJetTriggerFilter.cpp
    src/MPIMatchFilter.cpp
    src/PeriodWeights.cpp
    src/PileUpVars.cpp
    src/RunFilter.cpp
    src/Weights.cpp
    "${CMAKE_BINARY_DIR}/multijet-plugins_dict.cxx"
)
target_include_directories(multijet-plugins PUBLIC include)
target_link_libraries(multijet-plugins
    PUBLIC
        mensura::mensura mensura::mensura-pec
        ROOT::Hist ROOT::MathCore ROOT::Physics ROOT::Tree ROOT::TreePlayer
)


# Executables
add_executable(multijet prog/multijet.cpp)
target_link_libraries(multijet
    PRIVATE
        stdc++fs
        multijet-plugins
    PRIVATE
        Boost::boost Boost::program_options
)

add_executable(partition_runs prog/partition_runs.cpp)
target_include_directories(partition_runs
    PRIVATE "$ENV{MENSURA_INSTALL}/src/PECReader"
)
target_link_libraries(partition_runs
    PRIVATE
        stdc++fs
        multijet-plugins
        Boost::boost Boost::program_options
)

