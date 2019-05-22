#include <PeriodWeights.hpp>

#include <mensura/Processor.hpp>
#include <mensura/ROOTLock.hpp>

#include <utility>


namespace fs = std::filesystem;


PeriodWeights::PeriodWeights(std::string const &name, std::string const &configPath,
  std::string const &triggerName_):
    AnalysisPlugin(name),
    config(configPath),
    profilesDir(config.Get({"location"}).asString()),
    triggerName(triggerName_),
    fileServiceName("TFileService"), fileService(nullptr),
    puPluginName("PileUp"), puPlugin(nullptr),
    treeName(name)
{}


void PeriodWeights::BeginRun(Dataset const &dataset)
{
    // Construct pileup profile for the given data set. First try to find a dedicated profile file;
    // if it does not exist, use the default profile.
    fs::path profilePath("pileup_" + dataset.GetSourceDatasetID() + ".root");

    if (not fs::exists(profilesDir / profilePath))
        profilePath = config.Get({"default_sim_profile"}).asString();

    simPileupProfile.reset(ReadProfile(profilePath));


    ConstructPeriods();


    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    puPlugin = dynamic_cast<PileUpReader const *>(GetDependencyPlugin(puPluginName));

    
    // Create output tree
    tree = fileService->Create<TTree>(directoryName.c_str(), treeName.c_str(), "Event weights");
    
    ROOTLock::Lock();

    for (auto const &[periodLabel, period]: periods)
        tree->Branch(("Weight_" + periodLabel).c_str(), &period.weight);
    
    ROOTLock::Unlock();
}


PeriodWeights *PeriodWeights::Clone() const
{
    return new PeriodWeights(GetName(), config.FilePath(), triggerName);
}


void PeriodWeights::SetTreeName(std::string const &name)
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


void PeriodWeights::ConstructPeriods()
{
    auto const &periodConfigs = config.Get({"periods"});

    for (auto const &periodLabel: periodConfigs.getMemberNames())
    {
        auto const &periodConfig = Config::Get(periodConfigs, {periodLabel, triggerName});

        Period period;
        period.luminosity = Config::Get(periodConfig, {"lumi"}).asDouble();
        period.dataPileupProfile.reset(ReadProfile(
          Config::Get(periodConfig, {"pileup_profile"}).asString()));

        periods.emplace(std::make_pair(periodLabel, std::move(period)));
    }
}


bool PeriodWeights::ProcessEvent()
{
    double mu = puPlugin->GetExpectedPileUp();
    
    // Protection against a bug in sampling of pileup
    // [1] https://indico.cern.ch/event/695872/#43-feature-in-fall17-pile-up-d
    if (mu < 0.)
        mu = 0.;


    double const puProbSim = simPileupProfile->GetBinContent(simPileupProfile->FindFixBin(mu));

    for (auto const &[periodLabel, period]: periods)
    {
        if (puProbSim == 0.)
            period.weight = 0.;
        else
        {
            double const puProbData = period.dataPileupProfile->GetBinContent(
              period.dataPileupProfile->FindFixBin(mu));
            period.weight = puProbData / puProbSim * period.luminosity;
        }
    }


    tree->Fill();
    return true;
}


TH1 *PeriodWeights::ReadProfile(fs::path path)
{
    path = profilesDir / path;

    if (not fs::exists(path) or not fs::is_regular_file(path))
    {
        std::ostringstream message;
        message << "PeriodWeights[\"" << GetName() << "\"]::ReadProfile: File " << path <<
          " does not exist.";
        throw std::runtime_error(message.str());
    }

    TFile profileFile(path.c_str());
    auto *profile = dynamic_cast<TH1 *>(profileFile.Get("pileup"));

    if (not profile)
    {
        std::ostringstream message;
        message << "PeriodWeights[\"" << GetName() << "\"]::ReadProfile: File " << path <<
          " does not contain required histogram \"pileup\".";
        throw std::runtime_error(message.str());
    }

    profile->SetDirectory(nullptr);
    profileFile.Close();

    profile->Scale(1. / profile->Integral(0, -1), "width");
    return profile;
}

