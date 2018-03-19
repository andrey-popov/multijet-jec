add_executable(multijet multijet.cpp)
target_link_libraries(multijet multijet-plugins ${MENSURA_LIBRARIES} ${ROOT_LIBRARIES}
    ${Boost_FILESYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_executable(addWeights addWeights.cpp)
target_link_libraries(addWeights multijet-plugins ${MENSURA_LIBRARIES} ${ROOT_LIBRARIES}
    ${Boost_PROGRAM_OPTIONS_LIBRARY})