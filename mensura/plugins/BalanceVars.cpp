#include <BalanceVars.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <mensura/extensions/EventWeightPlugin.hpp>
#include <mensura/extensions/TFileService.hpp>

#include <mensura/PECReader/PECTriggerFilter.hpp>


BalanceVars::BalanceVars(std::string const name /*= "BalanceVars"*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
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
    recoilBuilder = dynamic_cast<RecoilBuilder const *>(GetDependencyPlugin(recoilBuilderName));
    
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
    // Save variables computed by the RecoilBuilder
    bfPtRecoil = recoilBuilder->GetP4Recoil().Pt();
    bfPtJ1 = recoilBuilder->GetP4LeadingJet().Pt();
    bfEtaJ1 = recoilBuilder->GetP4LeadingJet().Eta();
    bfA = recoilBuilder->GetA();
    bfAlpha = recoilBuilder->GetAlpha();
    bfBeta = recoilBuilder->GetBeta();
    bfTriggerBin = recoilBuilder->GetTriggerBin();
    
    
    // Compute variables reflecting balance in pt
    bfMJB = recoilBuilder->GetP4LeadingJet().Pt() / recoilBuilder->GetP4Recoil().Pt();
    
    
    // Compute event weight
    if (isMC)
    {
        double w = weightDataset;
        w *= triggerFilter->GetWeight();
        bfWeight[0] = w * puReweighter->GetWeight();
        bfWeight[1] = w * puReweighter->GetWeightUp(0);
        bfWeight[2] = w * puReweighter->GetWeightDown(0);
    }
    
    
    tree->Fill();
    return true;
}
