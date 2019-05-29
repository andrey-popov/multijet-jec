#include <GenWeights.hpp>

#include <mensura/Processor.hpp>
#include <mensura/ROOTLock.hpp>


GenWeights::GenWeights(std::string const &name):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    generatorPluginName(""), generatorPlugin(nullptr),
    treeName(name)
{}


void GenWeights::BeginRun(Dataset const &dataset)
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


GenWeights *GenWeights::Clone() const
{
    return new GenWeights(*this);
}


void GenWeights::SetTreeName(std::string const &name)
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


bool GenWeights::ProcessEvent()
{
    bfWeightGen = weightDataset;

    if (generatorPlugin)
    {
        double const nominal = generatorPlugin->GetNominalWeight();
        bfWeightGen *= nominal;

        // Indices of weights for scales variations in ME are set according to [1] (note that the
        // first weight from input MiniAOD file is not stored in PEC tuples)
        // [1] https://github.com/andrey-popov/multijet-jec/blob/969b9d530f2e60528a9eb2d2d37dcaaa205372e6/grid/python/MultijetJEC_cfg.py#L143-L145
        bfWeightMERenorm[0] = generatorPlugin->GetAltWeight(2) / nominal;
        bfWeightMERenorm[1] = generatorPlugin->GetAltWeight(5) / nominal;
        bfWeightMEFact[0] = generatorPlugin->GetAltWeight(0) / nominal;
        bfWeightMEFact[1] = generatorPlugin->GetAltWeight(1) / nominal;
    }

    tree->Fill();
    return true;
}

