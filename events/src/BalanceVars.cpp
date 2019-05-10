#include <BalanceVars.hpp>

#include <BalanceCalc.hpp>

#include <mensura/JetMETReader.hpp>
#include <mensura/Processor.hpp>
#include <mensura/ROOTLock.hpp>
#include <mensura/TFileService.hpp>

#include <TLorentzVector.h>
#include <TVector2.h>

#include <cmath>


BalanceVars::BalanceVars(std::string const &name, double minPtRecoil_):
    AnalysisPlugin(name),
    minPtRecoil(minPtRecoil_),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    balanceCalcName("BalanceCalc"), balanceCalc(nullptr),
    treeName(name)
{}


BalanceVars::BalanceVars(double minPtRecoil):
    BalanceVars("BalanceVars", minPtRecoil)
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
    balanceCalc = dynamic_cast<BalanceCalc const *>(GetDependencyPlugin(balanceCalcName));
    
    
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
    auto const &jets = jetmetPlugin->GetJets();
    auto const &met = jetmetPlugin->GetMET().P4();
    auto const &j1 = jets.at(0);
    
    
    bfPtJ1 = j1.Pt();
    bfPtJ2 = (jets.size() > 1) ? jets[1].Pt(): 0.;
    bfPtJ3 = (jets.size() > 2) ? jets[2].Pt(): 0.;
    
    bfMET = met.Pt();
    bfDPhi12 = (jets.size() > 1) ? std::abs(TVector2::Phi_mpi_pi(j1.Phi() - jets[1].Phi())) : 0.;
    
    
    TLorentzVector p4Recoil;
    
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        auto const &j = jets[i];
        
        if (j.Pt() < minPtRecoil)
            break;
        
        p4Recoil += j.P4();
    }
    
    bfPtRecoil = p4Recoil.Pt();
    
    
    bfPtBal = balanceCalc->GetPtBal();
    bfMPF = balanceCalc->GetMPF();
    
    
    tree->Fill();
    return true;
}
