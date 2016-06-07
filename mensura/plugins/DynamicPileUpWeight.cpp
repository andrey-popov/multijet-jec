#include <DynamicPileUpWeight.hpp>

#include <TriggerBin.hpp>

#include <mensura/core/FileInPath.hpp>
#include <mensura/core/PileUpReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <TKey.h>

#include <cstdlib>
#include <sstream>
#include <stdexcept>


DynamicPileUpWeight::DynamicPileUpWeight(std::string const &name,
  std::initializer_list<std::string> const &dataPUFileNames, std::string const &mcPUFileName,
  double systError_):
    EventWeightPlugin(name),
    puPluginName("PileUp"), puPlugin(nullptr),
    triggerBinPluginName("TriggerBin"), triggerBinPlugin(nullptr),
    systError(systError_)
{
    ROOTLock::Lock();
    
    // Read target (data) pile-up distributions
    for (auto const &dataPUFileName: dataPUFileNames)
    {
        TFile dataPUFile((FileInPath::Resolve("PileUp", dataPUFileName)).c_str());
        TH1 *dataPUHist = dynamic_cast<TH1 *>(dataPUFile.Get("pileup"));
        
        // Make sure the histogram is not associated with a file
        dataPUHist->SetDirectory(nullptr);
        
        // Normalize it to get a probability density and adjust the over/underflow bins
        dataPUHist->Scale(1. / dataPUHist->Integral(0, -1), "width");
        dataPUHist->SetBinContent(0, 0.);
        dataPUHist->SetBinContent(dataPUHist->GetNbinsX() + 1, 0.);
        
        dataPUHists.emplace_back(dataPUHist);
        dataPUFile.Close();
    }
    
    
    // Open file with distributions as generated
    mcPUFile.reset(new TFile((FileInPath::Resolve("PileUp", mcPUFileName)).c_str()));
    
    ROOTLock::Unlock();
}


DynamicPileUpWeight::DynamicPileUpWeight(std::initializer_list<std::string> const &dataPUFileNames,
  std::string const &mcPUFileName, double systError):
    DynamicPileUpWeight("PileUpWeight", dataPUFileNames, mcPUFileName, systError)
{}


void DynamicPileUpWeight::BeginRun(Dataset const &dataset)
{
    // Save pointers to required plugins
    puPlugin = dynamic_cast<PileUpReader const *>(GetDependencyPlugin(puPluginName));
    triggerBinPlugin = dynamic_cast<TriggerBin const *>(GetDependencyPlugin(triggerBinPluginName));
    
    
    // Update the simulated pile-up profile if needed
    std::string const simProfileLabel = dataset.GetSourceDatasetID();
    
    if (not mcPUHist or std::string(mcPUHist->GetName()) != simProfileLabel)
    {
        ROOTLock::Lock();
        
        // Try to read the desired profile from the file
        TList const *fileContent = mcPUFile->GetListOfKeys();
        TKey *key = dynamic_cast<TKey *>(fileContent->FindObject(simProfileLabel.c_str()));
        
        if (key)
        {
            mcPUHist.reset(dynamic_cast<TH1 *>(key->ReadObj()));
            mcPUHist->SetDirectory(nullptr);
        }
        else
        {
            // There is no specific histogram for the current dataset. Use the nominal one
            TH1 *nominalProfile = dynamic_cast<TH1 *>(mcPUFile->Get("nominal"));
            
            if (not nominalProfile)
            {
                std::ostringstream ost;
                ost << "DynamicPileUpWeight::BeginRun: File wile pile-up profiles \"" <<
                  mcPUFile->GetName() << "\" does not contain the required histogram \"nominal\".";
                throw std::runtime_error(ost.str());
            }
            
            mcPUHist.reset(nominalProfile);
            mcPUHist->SetDirectory(nullptr);
        }
        
        ROOTLock::Unlock();
        
        
        // Make sure the histogram is normalized
        mcPUHist->Scale(1. / mcPUHist->Integral(0, -1), "width");
    }
    
    
    // Initialize weights
    weights.assign(3, 0.);
}


Plugin *DynamicPileUpWeight::Clone() const
{
    return new DynamicPileUpWeight(*this);
}


bool DynamicPileUpWeight::ProcessEvent()
{
    // Read the expected number of pile-up events
    double const nTruth = puPlugin->GetExpectedPileUp();
    
    
    // Find probability according to MC histogram
    unsigned bin = mcPUHist->FindFixBin(nTruth);
    double const mcProb = mcPUHist->GetBinContent(bin);
    
    if (mcProb <= 0.)
        weights = {0., 0., 0.};
    
    
    // Determine what target profile should be used for this event
    auto const &dataPUHist = dataPUHists.at(triggerBinPlugin->GetTriggerBin() - 1);
    
    
    // Calculate the weights
    bin = dataPUHist->FindFixBin(nTruth);
    weights.at(0) = dataPUHist->GetBinContent(bin) / mcProb;
    
    bin = dataPUHist->FindFixBin(nTruth * (1. + systError));
    weights.at(1) = dataPUHist->GetBinContent(bin) / mcProb * (1. + systError);
    //^ The last multiplier is needed to correct for the total normalisation due to rescale in
    //the variable of integration. Same applies to the down weight below
    
    bin = dataPUHist->FindFixBin(nTruth * (1. - systError));
    weights.at(2) = dataPUHist->GetBinContent(bin) / mcProb * (1. - systError);
    
    
    // This plugin does not perform any filtering, so ProcessEvent always returns true
    return true;
}
