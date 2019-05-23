#include <L1TPrefiringWeights.hpp>

#include <mensura/Config.hpp>
#include <mensura/FileInPath.hpp>
#include <mensura/Processor.hpp>

#include <TFile.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>


std::array<double, 3> L1TPrefiringWeights::WeightCalc::ComputeWeights(std::vector<Jet> const &jets)
  const
{
    double const nominal = ComputeWeight(jets, Variation::Nominal);
    double const up = ComputeWeight(jets, Variation::Up);
    double const down = ComputeWeight(jets, Variation::Down);

    return {nominal, up, down};
}


double L1TPrefiringWeights::WeightCalc::ComputeWeight(
  std::vector<Jet> const &jets, Variation variation) const
{
    // Compute event weight as described in [1]
    // [1] https://twiki.cern.ch/twiki/bin/viewauth/CMS/L1ECALPrefiringWeightRecipe#Introduction
    double nonPrefiringProb = 1.;

    for (auto const &jet: jets)
        nonPrefiringProb *= (1. - GetPrefiringProbability(jet, variation));

    return nonPrefiringProb;
}


double L1TPrefiringWeights::WeightCalc::GetPrefiringProbability(
  Jet const &jet, Variation variation) const
{
    int const bin = prefiringMap->FindFixBin(jet.Eta(), jet.Pt());
    double const prob = prefiringMap->GetBinContent(bin);

    if (variation == Variation::Nominal)
        return prob;

    // A short-cut for empty bins
    if (prob == 0.)
        return 0.;

    double const relSystError = 0.2;
    double const error = std::sqrt(
      std::pow(prefiringMap->GetBinError(bin), 2) + std::pow(prob * relSystError, 2));

    if (variation == Variation::Up)
        return std::min(prob + error, 1.);
    else
        return std::max(prob - error, 0.);
}


L1TPrefiringWeights::L1TPrefiringWeights(std::string const &name, std::string const &configPath):
    AnalysisPlugin{name},
    jetmetPluginName{"JetMET"}, jetmetPlugin{nullptr},
    calcs{new std::vector<WeightCalc>}, periodLabelMap{new std::map<std::string, unsigned>}
{
    BuildCalcs(configPath);
}


L1TPrefiringWeights::L1TPrefiringWeights(std::string const &configPath):
    L1TPrefiringWeights{"L1TPrefiringWeights", configPath}
{}


void L1TPrefiringWeights::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


L1TPrefiringWeights *L1TPrefiringWeights::Clone() const
{
    return new L1TPrefiringWeights(*this);
}


unsigned L1TPrefiringWeights::FindPeriodIndex(std::string const &periodLabel) const
{
    auto const res = periodLabelMap->find(periodLabel);

    if (res == periodLabelMap->end())
    {
        std::ostringstream message;
        message << "L1TPrefiringWeights[\"" << GetName() << "\"]::FindPeriodIndex: "
          "Unknown period label \"" << periodLabel << "\".";
        throw std::out_of_range(message.str());
    }
    else
        return res->second;
}


void L1TPrefiringWeights::BuildCalcs(std::string const &configPath)
{
    Config config{configPath};
    auto const &periodConfigs = config.Get({"periods"});
    
    for (std::string const periodLabel: periodConfigs.getMemberNames())
    {
        std::string const location{
          Config::Get(periodConfigs, {periodLabel, "L1T_prefiring_map"}).asString()};
        auto const splitPos = location.find_first_of(":");

        if (splitPos == std::string::npos)
        {
            std::ostringstream message;
            message << "L1TPrefiringWeights[\"" << GetName() << "\"]::BuildCalcs: "
              "Failed to extract the in-file path from location \"" << location << "\".";
            throw std::runtime_error(message.str());
        }

        std::string const path{location.substr(0, splitPos)};
        std::string const inFilePath{location.substr(splitPos + 1)};


        TFile inputFile{FileInPath::Resolve(path).c_str()};
        auto *prefiringMap = dynamic_cast<TH1 *>(inputFile.Get(inFilePath.c_str()));

        if (not prefiringMap)
        {
            std::ostringstream message;
            message << "L1TPrefiringWeights[\"" << GetName() << "\"]::BuildCalcs: "
              "Failed to read histogram \"" << inFilePath << "\" from file \"" << path << "\".";
            throw std::runtime_error(message.str());
        }

        prefiringMap->SetDirectory(nullptr);
        inputFile.Close();

        calcs->emplace_back(prefiringMap);
        (*periodLabelMap)[periodLabel] = calcs->size() - 1;
    }
}


bool L1TPrefiringWeights::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    cachedWeights.clear();
    
    for (auto const &calc: *calcs)
        cachedWeights.emplace_back(calc.ComputeWeights(jets));

    return true;
}

