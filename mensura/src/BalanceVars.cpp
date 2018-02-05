#include <BalanceVars.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <mensura/extensions/TFileService.hpp>

#include <cmath>


BalanceVars::BalanceVars(std::string const name /*= "BalanceVars"*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    recoilBuilderName("RecoilBuilder"), recoilBuilder(nullptr),
    treeName(name)
{}


void BalanceVars::BeginRun(Dataset const &dataset)
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
    recoilBuilder = dynamic_cast<RecoilBuilder const *>(GetDependencyPlugin(recoilBuilderName));
    
    
    // Create output tree
    tree = fileService->Create<TTree>(directoryName.c_str(), treeName.c_str(),
      "Observables for multijet balance");
    
    
    // Assign branch addresses
    ROOTLock::Lock();
    
    tree->Branch("PtJ1", &bfPtJ1);
    tree->Branch("EtaJ1", &bfEtaJ1);
    tree->Branch("PtRecoil", &bfPtRecoil);
    tree->Branch("MET", &bfMET);
    
    tree->Branch("A", &bfA);
    tree->Branch("Alpha", &bfAlpha);
    
    tree->Branch("PtBal", &bfPtBal);
    tree->Branch("MPF", &bfMPF);
    
    if (isMC)
        tree->Branch("WeightDataset", &bfWeightDataset);
    
    ROOTLock::Unlock();
}


Plugin *BalanceVars::Clone() const
{
    return new BalanceVars(*this);
}


void BalanceVars::SetRecoilBuilderName(std::string const &name)
{
    recoilBuilderName = name;
}


void BalanceVars::SetTreeName(std::string const &name)
{
    auto const pos = name.rfind('/');
    
    if (pos != std::string::npos)
    {
        treeName = name.substr(pos + 1);
        directoryName = name.substr(0, pos);
    }
    else
    {
        treeName = name;
        directoryName = "";
    }
}


bool BalanceVars::ProcessEvent()
{
    auto const &j1 = recoilBuilder->GetP4LeadingJet();
    auto const &recoil = recoilBuilder->GetP4Recoil();
    auto const &met = jetmetPlugin->GetMET().P4();
    
    
    bfPtJ1 = j1.Pt();
    bfEtaJ1 = j1.Eta();
    bfMET = met.Pt();    
    
    bfPtRecoil = recoil.Pt();
    bfA = recoilBuilder->GetA();
    bfAlpha = recoilBuilder->GetAlpha();
    
    bfPtBal = recoil.Pt() * std::cos(bfAlpha) / j1.Pt();
    bfMPF = 1. + (met.Px() * j1.Px() + met.Py() * j1.Py()) / std::pow(j1.Pt(), 2);
    
    
    tree->Fill();
    return true;
}
