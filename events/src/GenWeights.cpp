#include <GenWeights.hpp>

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

    if (not generatorPluginName.empty())
    {
        tree->Branch("WeightMERenorm", bfWeightMERenorm, "WeightMERenorm[2]/F")->SetTitle(
          "Up and down variations in renormalization scale in ME");
        tree->Branch("WeightMEFact", bfWeightMEFact, "WeightMEFact[2]/F")->SetTitle(
          "Up and down variations in factorization scale in ME");
    }
    
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
    {
        double const nominal = generatorPlugin->GetNominalWeight();
        bfWeightGen *= nominal;

        // Indices of weights for scales variations in ME are taken from [1]
        // [1] https://github.com/andrey-popov/PEC-tuples/issues/86#issuecomment-481698177
        bfWeightMERenorm[0] = generatorPlugin->GetAltWeight(3) / nominal;
        bfWeightMERenorm[1] = generatorPlugin->GetAltWeight(6) / nominal;
        bfWeightMEFact[0] = generatorPlugin->GetAltWeight(1) / nominal;
        bfWeightMEFact[1] = generatorPlugin->GetAltWeight(2) / nominal;
    }

    tree->Fill();
    return true;
}

