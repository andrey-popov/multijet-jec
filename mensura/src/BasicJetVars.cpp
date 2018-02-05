#include <BasicJetVars.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <mensura/extensions/TFileService.hpp>

#include <cmath>


BasicJetVars::BasicJetVars(std::string const name /*= "BasicJetVars"*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr)
{}


void BasicJetVars::BeginRun(Dataset const &dataset)
{
    // Save dataset type and its common weight
    isMC = dataset.IsMC();
    
    if (isMC)
    {
        auto const &firstFile = dataset.GetFiles().front();
        bfWeightDataset = firstFile.GetWeight();
    }
    
    
    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    
    
    // Create output tree
    tree = fileService->Create<TTree>("", "Vars", "Observables describing jets");
    
    
    // Assign branch addresses
    ROOTLock::Lock();
    
    tree->Branch("PtJ1", &bfPtJ1);
    tree->Branch("PtJ2", &bfPtJ2);
    tree->Branch("EtaJ1", &bfEtaJ1);
    tree->Branch("EtaJ2", &bfEtaJ2);
    tree->Branch("Ht", &bfHt);
    
    if (isMC)
        tree->Branch("WeightDataset", &bfWeightDataset);
    
    ROOTLock::Unlock();
}


Plugin *BasicJetVars::Clone() const
{
    return new BasicJetVars(*this);
}


bool BasicJetVars::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    
    // Compute jet observables
    bfPtJ1 = bfPtJ2 = bfEtaJ1 = bfEtaJ2 = -10.;
    
    if (jets.size() > 0)
    {
        bfPtJ1 = jets.at(0).Pt();
        bfEtaJ1 = jets.at(0).Eta();
    }
    
    if (jets.size() > 1)
    {
        bfPtJ2 = jets.at(1).Pt();
        bfEtaJ2 = jets.at(1).Eta();
    }
    
    bfHt = 0.;
    
    for (auto const &j: jets)
        bfHt += j.Pt();
    
    
    tree->Fill();
    return true;
}
