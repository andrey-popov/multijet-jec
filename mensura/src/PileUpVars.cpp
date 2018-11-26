#include <PileUpVars.hpp>

#include <mensura/core/PileUpReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>

#include <mensura/extensions/TFileService.hpp>


PileUpVars::PileUpVars(std::string const name /*= "PileUpVars"*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    puPluginName("PileUp"), puPlugin(nullptr),
    treeName(name)
{}


void PileUpVars::BeginRun(Dataset const &dataset)
{
    isMC = dataset.IsMC();
    
    
    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    puPlugin = dynamic_cast<PileUpReader const *>(GetDependencyPlugin(puPluginName));
    
    
    // Create output tree
    tree = fileService->Create<TTree>(directoryName.c_str(), treeName.c_str(),
      "Observables describing pileup");
    
    
    // Assign branch addresses
    ROOTLock::Lock();
    
    tree->Branch("NumPV", &bfNumPV);
    tree->Branch("Rho", &bfRho);
    
    if (isMC)
        tree->Branch("LambdaPU", &bfLambdaPU);
    
    ROOTLock::Unlock();
}


Plugin *PileUpVars::Clone() const
{
    return new PileUpVars(*this);
}


void PileUpVars::SetTreeName(std::string const &name)
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


bool PileUpVars::ProcessEvent()
{
    bfNumPV = puPlugin->GetNumVertices();
    bfRho = puPlugin->GetRho();
    
    if (isMC)
        bfLambdaPU = puPlugin->GetExpectedPileUp();
    
    
    tree->Fill();
    return true;
}
