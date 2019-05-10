#include <DumpWeights.hpp>

#include <mensura/Processor.hpp>
#include <mensura/ROOTLock.hpp>
#include <mensura/TFileService.hpp>
#include <mensura/WeightCollector.hpp>

#include <mensura/PECReader/PECGeneratorReader.hpp>

#include <sstream>
#include <stdexcept>


using namespace std::string_literals;


DumpWeights::DumpWeights(std::string const &name,
  std::string const generatorPluginName_ /*= ""*/,
  std::string const weightCollectorName_ /*= ""*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    generatorPluginName(generatorPluginName_), generatorPlugin(nullptr),
    weightCollectorName(weightCollectorName_), weightCollector(nullptr)
{}


DumpWeights::DumpWeights(DumpWeights const &src):
    AnalysisPlugin(src),
    fileServiceName(src.fileServiceName), fileService(nullptr),
    generatorPluginName(src.generatorPluginName), generatorPlugin(nullptr),
    weightCollectorName(src.weightCollectorName), weightCollector(nullptr),
    systWeights(src.systWeights)
{}


void DumpWeights::BeginRun(Dataset const &dataset)
{
    if (not dataset.IsMC())
    {
        std::ostringstream message;
        message << "DumpWeights[\"" << GetName() << "\"]::BeginRun: The current dataset is data, "
          "but this plugin should only be used with simulation.";
        throw std::runtime_error(message.str());
    }
    
    
    // Save pointers to services and other plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    
    if (generatorPluginName != "")
        generatorPlugin =
          dynamic_cast<PECGeneratorReader const *>(GetDependencyPlugin(generatorPluginName));
    
    if (weightCollectorName != "")
        weightCollector =
          dynamic_cast<WeightCollector const *>(GetDependencyPlugin(weightCollectorName));
    
    
    // Adjust the size of the array to store alternative weights
    unsigned nSystWeights = 0;
    
    if (weightCollectorName != "")
        for (unsigned iPlugin = 0; iPlugin < weightCollector->GetNumPlugins(); ++iPlugin)
            nSystWeights += 2 * weightCollector->GetPlugin(iPlugin)->GetNumVariations();
    
    systWeights.resize(nSystWeights);
    
    
    // Create output tree
    tree = fileService->Create<TTree>("", "Weights", "Nominal and alternative weights");
    
    
    // Assign branch addresses
    ROOTLock::Lock();
    
    tree->Branch("weight", &weight);
    tree->Branch("systWeights", systWeights.data(),
      ("systWeights["s + std::to_string(systWeights.size()) + "]/F").c_str());
    
    ROOTLock::Unlock();
    
    
    // Common event weight in this dataset
    auto const &firstFile = dataset.GetFiles().front();
    weightDataset = firstFile.GetWeight();
}


Plugin *DumpWeights::Clone() const
{
    return new DumpWeights(*this);
}


bool DumpWeights::ProcessEvent()
{
    double w = weightDataset;
    
    if (generatorPlugin)
        w *= generatorPlugin->GetNominalWeight();
    
    
    if (weightCollector)
    {
        weight = w * weightCollector->GetWeight();
        unsigned curWeightIndex = 0;
        
        for (unsigned iPlugin = 0; iPlugin < weightCollector->GetNumPlugins(); ++iPlugin)
        {
            EventWeightPlugin const *plugin = weightCollector->GetPlugin(iPlugin);
            
            for (unsigned iVar = 0; iVar < plugin->GetNumVariations(); ++iVar)
            {
                systWeights.at(curWeightIndex) = w * weightCollector->GetWeightUp(iPlugin, iVar);
                systWeights.at(curWeightIndex + 1) =
                  w * weightCollector->GetWeightDown(iPlugin, iVar);
                curWeightIndex += 2;
            }
        }
    }
    else
        weight = w;
    
    
    tree->Fill();
    return true;
}
