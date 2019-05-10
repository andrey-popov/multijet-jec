#include "CandMapCountFilter.h"

#include <FWCore/Framework/interface/MakerMacros.h>


using namespace edm;
using namespace std;


CandMapCountFilter::CandMapCountFilter(ParameterSet const &cfg):
    minNumber(cfg.getParameter<unsigned>("minNumber")),
    maxNumber(cfg.getParameter<unsigned>("maxNumber"))
{
    // Register input data
    collectionToken = consumes<View<reco::Candidate>>(cfg.getParameter<InputTag>("src"));
    mapToken = consumes<ValueMap<bool>>(cfg.getParameter<InputTag>("acceptMap"));
}


void CandMapCountFilter::fillDescriptions(ConfigurationDescriptions &descriptions)
{
    ParameterSetDescription desc;
    desc.add<InputTag>("src")->setComment("Input collection.");
    desc.add<InputTag>("acceptMap")->
     setComment("Map with boolean accept decisions.");
    desc.add<unsigned>("minNumber", 0)->
     setComment("Minimal allowed number of accepted objects.");
     desc.add<unsigned>("maxNumber", 999)->
     setComment("Maximal allowed number of accepted objects.");
    
    descriptions.add("CandMapCountFilter", desc);
}


bool CandMapCountFilter::filter(edm::Event &event, edm::EventSetup const &)
{
    // Read the input collection and the accept map
    Handle<View<reco::Candidate>> collection;
    Handle<ValueMap<bool>> map;
    
    event.getByToken(collectionToken, collection);
    event.getByToken(mapToken, map);
    
    
    // Loop over the collection and count accepted objects
    unsigned nAccepted = 0;
    
    for (unsigned i = 0; i < collection->size(); ++i)
    {
        Ptr<reco::Candidate> const candPtr(collection, i);
        
        if ((*map)[candPtr])
            ++nAccepted;
    }
    
    
    return (nAccepted >= minNumber and nAccepted <= maxNumber);
}


DEFINE_FWK_MODULE(CandMapCountFilter);
