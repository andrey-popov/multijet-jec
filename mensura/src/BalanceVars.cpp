#include <BalanceVars.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <mensura/extensions/TFileService.hpp>

#include <TVector2.h>

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
    tree->Branch("PtJ2", &bfPtJ2);
    tree->Branch("PtJ3", &bfPtJ3);
    
    tree->Branch("PtRecoil", &bfPtRecoil);
    tree->Branch("MET", &bfMET);
    tree->Branch("DPhi12", &bfDPhi12);
    
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


double BalanceVars::ComputeMPF(TLorentzVector const &p4Lead, TLorentzVector const &p4Miss)
{
    return 1. + (p4Miss.Px() * p4Lead.Px() + p4Miss.Py() * p4Lead.Py()) / std::pow(p4Lead.Pt(), 2);
}


double BalanceVars::ComputePtBal(TLorentzVector const &p4Lead, TLorentzVector const &p4Recoil)
{
    return -(p4Lead.Px() * p4Recoil.Px() + p4Lead.Py() * p4Recoil.Py()) / std::pow(p4Lead.Pt(), 2);
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
    auto const &jets = jetmetPlugin->GetJets();
    
    
    bfPtJ1 = j1.Pt();
    bfPtJ2 = (jets.size() > 1) ? jets[1].Pt(): 0.;
    bfPtJ3 = (jets.size() > 2) ? jets[2].Pt(): 0.;
    
    bfMET = met.Pt();
    bfPtRecoil = recoil.Pt();
    bfDPhi12 = (jets.size() > 1) ? std::abs(TVector2::Phi_mpi_pi(j1.Phi() - jets[1].Phi())) : 0.;
    
    bfPtBal = ComputePtBal(j1, recoil);
    bfMPF = ComputeMPF(j1, met);
    
    
    tree->Fill();
    return true;
}
