ROOT_GENERATE_DICTIONARY(multijet-plugins_dict PhysicsObjects.hpp
    LINKDEF classes.h classes_def.xml NOINSTALL)

add_library(multijet-plugins SHARED
    BalanceFilter.cpp BalanceHists.cpp BalanceVars.cpp BasicJetVars.cpp DumpEventID.cpp
    DumpWeights.cpp EtaPhiFilter.cpp FirstJetFilter.cpp GenMatchFilter.cpp JERCJetMETReader.cpp
    JetIDFilter.cpp LeadJetTriggerFilter.cpp PileUpVars.cpp RecoilBuilder.cpp RunFilter.cpp
    multijet-plugins_dict.cxx)
target_link_libraries(multijet-plugins ${MENSURA_LIBRARIES} ${ROOT_LIBRARIES})
