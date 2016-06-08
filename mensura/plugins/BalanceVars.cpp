#include <BalanceVars.hpp>

#include <RecoilBuilder.hpp>
#include <TriggerBin.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <mensura/extensions/EventWeightPlugin.hpp>
#include <mensura/extensions/TFileService.hpp>

#include <mensura/PECReader/PECTriggerFilter.hpp>

#include <cmath>


BalanceVars::BalanceVars(std::string const name /*= "BalanceVars"*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    triggerBinPluginName("TriggerBin"), triggerBinPlugin(nullptr),
    recoilBuilderName("RecoilBuilder"), recoilBuilder(nullptr),
    triggerFilterName("TriggerFilter"), triggerFilter(nullptr),
    puReweighterName("PileUpWeight"), puReweighter(nullptr)
{}


void BalanceVars::BeginRun(Dataset const &dataset)
{
    // Save dataset type and its common weight
    isMC = dataset.IsMC();
    weightDataset = 1.;
    
    if (isMC)
    {
        auto const &firstFile = dataset.GetFiles().front();
        weightDataset = firstFile.xSec / firstFile.nEvents;
    }
    
    
    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    recoilBuilder = dynamic_cast<RecoilBuilder const *>(GetDependencyPlugin(recoilBuilderName));
    triggerBinPlugin = dynamic_cast<TriggerBin const *>(GetDependencyPlugin(triggerBinPluginName));
    
    if (isMC)
    {
        triggerFilter =
          dynamic_cast<PECTriggerFilter const *>(GetDependencyPlugin(triggerFilterName));
        puReweighter =
          dynamic_cast<EventWeightPlugin const *>(GetDependencyPlugin(puReweighterName));
    }
    
    
    // Create output tree
    tree = fileService->Create<TTree>("", "Vars", "Observables for multijet balance");
    
    
    // Assign branch addresses
    ROOTLock::Lock();
    
    tree->Branch("PtRecoil", &bfPtRecoil);
    tree->Branch("PtJ1", &bfPtJ1);
    tree->Branch("EtaJ1", &bfEtaJ1);
    tree->Branch("A", &bfA);
    tree->Branch("Alpha", &bfAlpha);
    tree->Branch("Beta", &bfBeta);
    tree->Branch("TriggerBin", &bfTriggerBin);
    
    tree->Branch("MJB", &bfMJB);
    tree->Branch("MPF", &bfMPF);
    tree->Branch("CRecoil", &bfCRecoil);
    
    if (isMC)
        tree->Branch("Weight", bfWeight, "Weight[3]/F");
    
    ROOTLock::Unlock();
}


Plugin *BalanceVars::Clone() const
{
    return new BalanceVars(*this);
}


bool BalanceVars::ProcessEvent()
{
    auto const &recoil = recoilBuilder->GetP4Recoil();
    auto const &j1 = recoilBuilder->GetP4LeadingJet();
    auto const &jets = jetmetPlugin->GetJets();
    auto const &met = jetmetPlugin->GetMET().P4();
    
    
    // Save variables computed by the RecoilBuilder
    bfPtRecoil = recoil.Pt();
    bfPtJ1 = j1.Pt();
    bfEtaJ1 = j1.Eta();
    bfA = recoilBuilder->GetA();
    bfAlpha = recoilBuilder->GetAlpha();
    bfBeta = recoilBuilder->GetBeta();
    bfTriggerBin = triggerBinPlugin->GetTriggerBin();
    
    
    // Compute variables reflecting balance in pt. See Sec. 2 in AN-14-016 for definitions
    bfMJB = j1.Pt() / recoil.Pt();
    bfMPF = 1. + (met.Px() * recoil.Px() + met.Py() * recoil.Py()) / std::pow(recoil.Pt(), 2);
    
    
    // Compute C_recoil. Defined according to Eqs. (21), (23) in JME-13-004. See also [1]
    //[1] https://github.com/pequegnot/multijetAnalysis/blob/ff65f3db37189383f4b61d27b1e8f20c4c89d26f/weightPlots/multijet_weight_common.cpp#L1293-L1303
    double sum = 0.;
    
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        auto const &j = jets[i];
        
        if (j.Pt() < recoilBuilder->GetJetPtThreshold())
            break;
        
        double const f = j.Pt() / recoil.Pt();
        sum += f * std::log(f) * std::cos(j.Phi() - recoil.Phi());
    }
    
    bfCRecoil = std::exp(sum);
    
    
    // Compute event weight
    if (isMC)
    {
        double const w = weightDataset * triggerFilter->GetWeight();
        bfWeight[0] = w * puReweighter->GetWeight();
        bfWeight[1] = w * puReweighter->GetWeightUp(0);
        bfWeight[2] = w * puReweighter->GetWeightDown(0);
    }
    
    
    tree->Fill();
    return true;
}
