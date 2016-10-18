#include <BalanceVars.hpp>

#include <RecoilBuilder.hpp>
#include <TriggerBin.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <mensura/extensions/TFileService.hpp>

#include <cmath>


BalanceVars::BalanceVars(std::string const name /*= "BalanceVars"*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    triggerBinPluginName("TriggerBin"), triggerBinPlugin(nullptr),
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
    triggerBinPlugin = dynamic_cast<TriggerBin const *>(GetDependencyPlugin(triggerBinPluginName));
    
    
    // Create output tree
    tree = fileService->Create<TTree>("", treeName.c_str(), "Observables for multijet balance");
    
    
    // Assign branch addresses
    ROOTLock::Lock();
    
    tree->Branch("PtRecoil", &bfPtRecoil);
    tree->Branch("PtJ1", &bfPtJ1);
    tree->Branch("EtaJ1", &bfEtaJ1);
    
    tree->Branch("MET", &bfMET);
    tree->Branch("MultRecoil", &bfMultRecoil);
    tree->Branch("MeanRecoilJetPt", &bfMeanRecoilJetPt);
    
    tree->Branch("A", &bfA);
    tree->Branch("Alpha", &bfAlpha);
    tree->Branch("Beta", &bfBeta);
    tree->Branch("TriggerBin", &bfTriggerBin);
    
    tree->Branch("MJB", &bfMJB);
    tree->Branch("MPF", &bfMPF);
    tree->Branch("F_LogLinear", &bfFLogLinear);
    
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
    treeName = name;
}


bool BalanceVars::ProcessEvent()
{
    auto const &recoil = recoilBuilder->GetP4Recoil();
    auto const &j1 = recoilBuilder->GetP4LeadingJet();
    auto const &recoilJets = recoilBuilder->GetRecoilJets();
    auto const &met = jetmetPlugin->GetMET().P4();
    
    
    // Save basic observables
    bfPtJ1 = j1.Pt();
    bfEtaJ1 = j1.Eta();
    bfMET = met.Pt();    
    
    
    // Save variables computed by the RecoilBuilder
    bfPtRecoil = recoil.Pt();
    bfA = recoilBuilder->GetA();
    bfAlpha = recoilBuilder->GetAlpha();
    bfBeta = recoilBuilder->GetBeta();
    bfTriggerBin = triggerBinPlugin->GetTriggerBin();
    
    
    // Compute variables reflecting balance in pt. See Sec. 2 in AN-14-016 for definitions
    // bfMJB = j1.Pt() * std::cos(bfAlpha) / recoil.Pt();
    bfMJB = j1.Pt() / recoil.Pt();
    bfMPF = 1. + (met.Px() * recoil.Px() + met.Py() * recoil.Py()) / std::pow(recoil.Pt(), 2);
    
    
    // Compute F a.k.a. C_recoil. For log-linear JEC it is defined according to Eqs. (21), (23) in
    //JME-13-004. See also [1].
    //[1] https://github.com/pequegnot/multijetAnalysis/blob/ff65f3db37189383f4b61d27b1e8f20c4c89d26f/weightPlots/multijet_weight_common.cpp#L1293-L1303
    double sumLogLinear = 0.;
    
    for (Jet const &j: recoilJets)
    {
        double const f = j.Pt() / recoil.Pt();
        double const cosDPhi = std::cos(j.Phi() - recoil.Phi());
        
        sumLogLinear += f * std::log(f) * cosDPhi;
    }
    
    bfFLogLinear = std::exp(sumLogLinear);
    
    
    // Compute more properties of jets in the recoil
    bfMultRecoil = 0;
    double sumPt = 0.;
    
    for (Jet const &j: recoilJets)
    {
        ++bfMultRecoil;
        sumPt += j.Pt();
    }
    
    bfMeanRecoilJetPt = sumPt / bfMultRecoil;
    
    
    tree->Fill();
    return true;
}
