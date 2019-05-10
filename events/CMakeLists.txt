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


# Main library
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
    src/JERCJetMETUpdate.cpp
    src/JetIDFilter.cpp
    src/LeadJetTriggerFilter.cpp
    src/MPIMatchFilter.cpp
    src/PileUpVars.cpp
    src/RunFilter.cpp
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
        multijet-plugins
    PRIVATE
        Boost::boost Boost::program_options
)

add_executable(add_weights prog/addWeights.cpp)
target_link_libraries(add_weights
    PRIVATE
        multijet-plugins
    PRIVATE
        Boost::boost Boost::program_options
)

