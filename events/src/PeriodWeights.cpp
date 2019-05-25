#include <PeriodWeights.hpp>

#include <mensura/Processor.hpp>
#include <mensura/ROOTLock.hpp>

#include <limits>
#include <utility>


namespace fs = std::filesystem;


PeriodWeights::Period::Period() noexcept:
    weight{std::numeric_limits<Float_t>::quiet_NaN()},
    prefiringWeightNominal{std::numeric_limits<Float_t>::quiet_NaN()},
    prefiringWeightSyst{
      std::numeric_limits<Float_t>::quiet_NaN(), std::numeric_limits<Float_t>::quiet_NaN()}
{}


PeriodWeights::PeriodWeights(std::string const &name, std::string const &configPath,
  std::string const &triggerName_):
    AnalysisPlugin(name),
    config(configPath),
    profilesDir(config.Get({"pileup_profiles_location"}).asString()),
    triggerName(triggerName_),
    fileServiceName("TFileService"), fileService(nullptr),
    puPluginName("PileUp"), puPlugin(nullptr),
    prefiringPluginName(""), prefiringPlugin(nullptr),
    treeName(name)
{}


void PeriodWeights::BeginRun(Dataset const &dataset)
{
    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    puPlugin = dynamic_cast<PileUpReader const *>(GetDependencyPlugin(puPluginName));

    if (not prefiringPluginName.empty())
        prefiringPlugin = dynamic_cast<L1TPrefiringWeights const *>(
          GetDependencyPlugin(prefiringPluginName));


    // Construct pileup profile for the given data set. First try to find a dedicated profile file;
    // if it does not exist, use the default profile.
    fs::path profilePath("pileup_" + dataset.GetSourceDatasetID() + ".root");

    if (not fs::exists(profilesDir / profilePath))
        profilePath = config.Get({"default_sim_pileup_profile"}).asString();

    simPileupProfile.reset(ReadProfile(profilePath));


    ConstructPeriods();


    // Create output tree
    tree = fileService->Create<TTree>(directoryName.c_str(), treeName.c_str(), "Event weights");
    
    ROOTLock::Lock();

    for (auto const &[periodLabel, period]: periods)
    {
        tree->Branch(("Weight_" + periodLabel).c_str(), &period.weight);

        if (prefiringPlugin)
        {
            tree->Branch(("Weight_" + periodLabel + "_L1TPrefiring").c_str(),
              &period.prefiringWeightNominal)->SetTitle("Nominal prefiring weight");
            tree->Branch(("Weight_" + periodLabel + "_L1TPrefiringSyst").c_str(),
              period.prefiringWeightSyst)->SetTitle(
              "Relative up and down variations for prefiring weight");
        }
    }
    
    ROOTLock::Unlock();
}


PeriodWeights *PeriodWeights::Clone() const
{
    return new PeriodWeights(GetName(), config.FilePath(), triggerName);
}


void PeriodWeights::SetPrefiringWeightPlugin(std::string const &name)
{
    prefiringPluginName = name;
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
        auto const &periodConfig = Config::Get(periodConfigs, {periodLabel});
        auto const &periodTriggerConfig = Config::Get(periodConfig, {"triggers", triggerName});

        Period period;
        period.luminosity = Config::Get(periodTriggerConfig, {"lumi"}).asDouble();
        period.dataPileupProfile.reset(ReadProfile(
          Config::Get(periodTriggerConfig, {"pileup_profile"}).asString()));

        if (prefiringPlugin)
            period.index = prefiringPlugin->FindPeriodIndex(periodLabel);
        else
            period.index = -1;

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
        double puWeight;

        if (puProbSim == 0.)
            puWeight = 0.;
        else
        {
            double const puProbData = period.dataPileupProfile->GetBinContent(
              period.dataPileupProfile->FindFixBin(mu));
            puWeight = puProbData / puProbSim;
        }

        period.weight = period.luminosity * puWeight;


        // Save prefiring weights. Systematic variations are stored as relative variations with
        // respect to the nominal prefiring weight.
        if (prefiringPlugin)
        {
            auto const srcPrefiringWeights = prefiringPlugin->GetWeights(period.index);

            period.prefiringWeightNominal = srcPrefiringWeights[0];
            period.prefiringWeightSyst[0] = srcPrefiringWeights[1] / srcPrefiringWeights[0];
            period.prefiringWeightSyst[1] = srcPrefiringWeights[2] / srcPrefiringWeights[0];
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

