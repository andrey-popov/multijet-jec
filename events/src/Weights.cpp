#include <Weights.hpp>

#include <mensura/Processor.hpp>
#include <mensura/ROOTLock.hpp>


Weights::Weights(std::string const &name):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    generatorPluginName(""), generatorPlugin(nullptr),
    treeName(name)
{}


void Weights::BeginRun(Dataset const &dataset)
{
    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));

    if (not generatorPluginName.empty())
        generatorPlugin = dynamic_cast<PECGeneratorReader const *>(
          GetDependencyPlugin(generatorPluginName));
    
    
    // Create output tree
    tree = fileService->Create<TTree>(directoryName.c_str(), treeName.c_str(), "Event weights");
    
    ROOTLock::Lock();
    
    tree->Branch("WeightGen", &bfWeightGen)->SetTitle(
      "Full generator-level weight: sigma * w_i / sum_j(w_j)");
    
    ROOTLock::Unlock();


    // Save the data set weight, which is the same for all events in the data set
    weightDataset = dataset.GetWeight();
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
    bfWeightGen = weightDataset;

    if (generatorPlugin)
        bfWeightGen *= generatorPlugin->GetNominalWeight();

    tree->Fill();
    return true;
}

