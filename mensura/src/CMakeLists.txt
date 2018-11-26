add_library(multijet-plugins SHARED
    AngularFilter.cpp BalanceCalc.cpp BalanceFilter.cpp BalanceHists.cpp BalanceVars.cpp
    BasicJetVars.cpp DumpEventID.cpp DumpWeights.cpp EtaPhiFilter.cpp FirstJetFilter.cpp
    GenMatchFilter.cpp JetIDFilter.cpp LeadJetTriggerFilter.cpp MPIMatchFilter.cpp PileUpVars.cpp
    RecoilBuilder.cpp RunFilter.cpp)
target_link_libraries(multijet-plugins ${MENSURA_LIBRARIES} ${ROOT_LIBRARIES})
