#include <Weights.hpp>

#include <mensura/Processor.hpp>
#include <mensura/ROOTLock.hpp>


Weights::Weights(std::string const &name):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    treeName(name)
{}


void Weights::BeginRun(Dataset const &dataset)
{
    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    
    
    // Create output tree
    tree = fileService->Create<TTree>(directoryName.c_str(), treeName.c_str(), "Event weights");
    
    ROOTLock::Lock();
    
    tree->Branch("WeightSample", &bfWeightSample);
    
    ROOTLock::Unlock();


    // The sample weight is identical for all events in the data set. Set it here.
    bfWeightSample = dataset.GetWeight();
}


Weights *Weights::Clone() const
{
    return new Weights(*this);
}


void Weights::SetTreeName(std::string const &name)
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


bool Weights::ProcessEvent()
{
    tree->Fill();
    return true;
}

