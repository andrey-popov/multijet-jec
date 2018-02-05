#pragma once

#include <Analysis/Multijet/interface/PhysicsObjects.h>

#include <FWCore/Framework/interface/EDAnalyzer.h>
#include <FWCore/Framework/interface/Event.h>
#include <FWCore/ParameterSet/interface/ParameterSet.h>
#include <FWCore/ParameterSet/interface/ConfigurationDescriptions.h>
#include <FWCore/ParameterSet/interface/ParameterSetDescription.h>

#include <DataFormats/PatCandidates/interface/Jet.h>
#include <DataFormats/PatCandidates/interface/MET.h>

#include <FWCore/ServiceRegistry/interface/Service.h>
#include <CommonTools/UtilAlgos/interface/TFileService.h>

#include <TTree.h>

#include <string>
#include <vector>
#include <memory>


/**
 * \class BasicJetMET
 * \brief Stores reconstructed jets and MET.
 * 
 * This plugin stores basic properties of jets (four-momenta, b-tagging discriminators, IDs, etc.)
 * and MET. Fields with generator-level information are not filled when processing data.
 * 
 * The input collection of jets must have been created by an instance of plugin JERCJetSelector
 * as the plugin reads some userData from it, such as matching to generator-level jets. Raw jet
 * momenta are stored.
 * 
 * Raw CHS missing pt is stored.
 */
class BasicJetMET: public edm::EDAnalyzer
{
private:
    /// Supported versions of jet ID
    enum class JetID
    {
        Ver2016,
        Ver2017
    };
    
public:
    /**
     * \brief Constructor
     * 
     * Reads the configuration of the plugin and create string-based selectors.
     */
    BasicJetMET(edm::ParameterSet const &cfg);
    
public:
    /// Verifies configuration of the plugin
    static void fillDescriptions(edm::ConfigurationDescriptions &descriptions);
    
    /// Creates output tree
    virtual void beginJob() override;
    
    /**
     * \brief Analyses current event
     * 
     * Copies jets and MET into the buffers, evaluates string-based selections for jets, fills the
     * output tree.
     */
    virtual void analyze(edm::Event const &event, edm::EventSetup const &) override;
    
private:
    /// Collection of jets
    edm::EDGetTokenT<edm::View<pat::Jet>> jetToken;
    
    /// MET
    edm::EDGetTokenT<edm::View<pat::MET>> metToken;
    
    /**
     * \brief Indicates whether an event is data or simulation
     * 
     * Determines if the plugin should read generator-level information for jets, such as flavour.
     */
    bool const runOnData;
    
    /// Version of jet ID to be evaluated
    JetID jetIDVersion;
    
    /// An object to handle the output ROOT file
    edm::Service<TFileService> fileService;
    
    
    /// Output tree
    TTree *outTree;
    
    /// Non-owning pointer to a buffer to store jets
    std::vector<jec::Jet> *storeJets;
    
    /// Non-owning pointer to a buffer to store missing pt
    jec::MET *storeMET;
};
