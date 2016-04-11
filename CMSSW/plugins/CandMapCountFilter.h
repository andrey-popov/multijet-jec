#pragma once

#include <FWCore/Framework/interface/EDFilter.h>
#include <FWCore/Framework/interface/Event.h>
#include <FWCore/ParameterSet/interface/ParameterSet.h>
#include <FWCore/ParameterSet/interface/ConfigurationDescriptions.h>
#include <FWCore/ParameterSet/interface/ParameterSetDescription.h>

#include <DataFormats/Candidate/interface/Candidate.h>
#include <DataFormats/Common/interface/ValueMap.h>


/**
 * \class CandMapCountFilter
 * \brief A plugin that performs filtering based on the number of objects accepted in a boolean map
 * 
 * This plugin reads an input collection of objects inheriting from reco::Candidate and a boolean
 * map associated with it. It counts objects for which the mapped value is true and performs event
 * filtering based on their number. The foreseen use case is to perform filtering on the number of
 * well-identified objects when the IDs are only provided as a map.
 */
class CandMapCountFilter: public edm::EDFilter
{
public:
    /// Constructor based on the provided configuration
    CandMapCountFilter(edm::ParameterSet const &cfg);
    
public:
    /// Verifies configuration of the plugin
    static void fillDescriptions(edm::ConfigurationDescriptions &descriptions);
    
private:
    /// Performs event filtering
    virtual bool filter(edm::Event &event, edm::EventSetup const &) override;
    
private:
    /// Input collection
    edm::EDGetTokenT<edm::View<reco::Candidate>> collectionToken;
    
    /// Map with filter decisions
    edm::EDGetTokenT<edm::ValueMap<bool>> mapToken;
    
    /// Allowed range for the number of objects accepted by decision stored in the map
    unsigned minNumber, maxNumber;
};
