#include <BalanceHists.hpp>
#include <BalanceVars.hpp>
#include <DatasetScaler.hpp>
#include <FirstJetFilter.hpp>
#include <GenMatchFilter.hpp>
#include <JetIDFilter.hpp>
#include <PileUpVars.hpp>
#include <RecoilBuilder.hpp>
#include <TriggerBin.hpp>

#include <mensura/core/Dataset.hpp>
#include <mensura/core/FileInPath.hpp>
#include <mensura/core/RunManager.hpp>
#include <mensura/core/SystService.hpp>

#include <mensura/extensions/DatasetBuilder.hpp>
#include <mensura/extensions/JetCorrectorService.hpp>
#include <mensura/extensions/JetFilter.hpp>
#include <mensura/extensions/JetMETUpdate.hpp>
#include <mensura/extensions/PileUpWeight.hpp>
#include <mensura/extensions/TFileService.hpp>

#include <mensura/PECReader/PECGenJetMETReader.hpp>
#include <mensura/PECReader/PECInputData.hpp>
#include <mensura/PECReader/PECJetMETReader.hpp>
#include <mensura/PECReader/PECPileUpReader.hpp>
#include <mensura/PECReader/PECTriggerObjectReader.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>
#include <regex>


using namespace std;
namespace po = boost::program_options;


enum class DatasetGroup
{
    Data,
    MC
};


enum class Era
{
    All,
    Run2016BCD,
    Run2016EFearly,
    Run2016FlateG,
    Run2016H
};


int main(int argc, char **argv)
{
    // Parse arguments
    po::options_description options("Allowed options");
    options.add_options()
      ("help,h", "Prints help message")
      ("datasetGroup", po::value<string>(), "Dataset group (required)")
      ("add-jec", po::value<string>(), "Additional jet correction")
      ("pt-cuts,c", po::value<string>(), "Jet pt cuts")
      ("syst,s", po::value<string>(), "Systematic shift")
      ("wide", "Loosen selection to |eta(j1)| < 2.4");
    //^ This is some severe abuse of C++ syntax...
    
    po::positional_options_description positionalOptions;
    positionalOptions.add("datasetGroup", 1);
    
    po::variables_map optionsMap;
    
    po::store(
      po::command_line_parser(argc, argv).options(options).positional(positionalOptions).run(),
      optionsMap);
    po::notify(optionsMap);
    
    if (optionsMap.count("help"))
    {
        cerr << "Produces tuples with observables for the multijet method.\n";
        cerr << "Usage: multijet datasetGroup [options]\n";
        cerr << options << endl;
        return EXIT_FAILURE;
    }
    
    
    if (not optionsMap.count("datasetGroup"))
    {
        cerr << "Required argument datasetGroup is missing.\n";
        return EXIT_FAILURE;
    }
    
    
    string dataGroupText(optionsMap["datasetGroup"].as<string>());
    DatasetGroup dataGroup;
    
    if (dataGroupText == "pseudodata")
        dataGroup = DatasetGroup::Data;
    else if (dataGroupText == "mc" or dataGroupText == "sim")
        dataGroup = DatasetGroup::MC;
    else
    {
        cerr << "Cannot recognize dataset group \"" << dataGroupText << "\".\n";
        return EXIT_FAILURE;
    }
    
    
    // Parse list of jet pt cuts used to construct the recoil. Their values are assumed to be
    //integer.
    list<unsigned> jetPtCuts;
    
    if (optionsMap.count("pt-cuts"))
    {
        istringstream cutList(optionsMap["pt-cuts"].as<string>());
        string cut;
        
        while (getline(cutList, cut, ','))
            jetPtCuts.emplace_back(stod(cut));
    }
    else
        jetPtCuts = {15};
    
    
    // Parse requested systematic uncertainty
    string systType("None");
    SystService::VarDirection systDirection = SystService::VarDirection::Undefined;
    
    if (optionsMap.count("syst"))
    {
        string systArg(optionsMap["syst"].as<string>());
        boost::to_lower(systArg);
        
        std::regex systRegex("(jec|jer|metuncl)[-_]?(up|down)", std::regex::extended);
        std::smatch matchResult;
        
        if (not std::regex_match(systArg, matchResult, systRegex))
        {
            cerr << "Cannot recognize systematic variation \"" << systArg << "\".\n";
            return EXIT_FAILURE;
        }
        else
        {
            if (matchResult[1] == "jec")
                systType = "JEC";
            else if (matchResult[1] == "jer")
                systType = "JER";
            else if (matchResult[1] == "metuncl")
                systType = "METUncl";
            
            if (matchResult[2] == "up")
                systDirection = SystService::VarDirection::Up;
            else if (matchResult[2] == "down")
                systDirection = SystService::VarDirection::Down;
        }
    }
    
    
    // Input datasets
    list<Dataset> datasets;
    DatasetBuilder datasetBuilder("/gridgroup/cms/popov/Analyses/JetMET/"
      "2017.04.21_Grid-campaign-03Feb2017-fix/Results/samples_v1.json");
    
    if (dataGroup == DatasetGroup::Data)
    {
        // In case of pseudodata do not use extension datasets for high-Ht bins since there are
        //plenty of events
        datasets = datasetBuilder({"QCD-Ht-1000-1500-mg_all", "QCD-Ht-1500-2000-mg_fjr",
          "QCD-Ht-2000-inf-mg_NPb"});
    }
    else
        datasets = datasetBuilder({"QCD-Ht-1000-1500-mg_all", "QCD-Ht-1500-2000-mg_all",
          "QCD-Ht-2000-inf-mg_all"});
    
    
    // Add an additional locations to seach for data files
    char const *installPath = getenv("MULTIJET_JEC_INSTALL");
    
    if (not installPath)
    {
        cerr << "Mandatory environmental variable MULTIJET_JEC_INSTALL is not defined.\n";
        return EXIT_FAILURE;
    }
    
    FileInPath::AddLocation(string(installPath) + "/data/");
    
    
    // Construct the run manager
    RunManager manager(datasets.begin(), datasets.end());
    
    
    // Register services and plugins
    ostringstream outputNameStream;
    outputNameStream << "output/" << ((dataGroup == DatasetGroup::Data) ? "pseudodata" : "sim");
    
    if (systType != "None")
        outputNameStream << "_" << systType << "_" <<
          ((systDirection == SystService::VarDirection::Up) ? "up" : "down");
    
    outputNameStream << "/%";
    
    manager.RegisterService(new TFileService(outputNameStream.str()));
    
    manager.RegisterPlugin(new PECInputData);
    
    // Stochastic event filtering for pseudodata
    if (dataGroup == DatasetGroup::Data)
        manager.RegisterPlugin(new DatasetScaler(5e3, 6642));
    
    manager.RegisterPlugin(new PECPileUpReader);
    
    
    // Determine JEC version
    string const jecVersion("Summer16_03Feb2017_V1");
    
    
    manager.RegisterService(new SystService(systType, systDirection));
    manager.RegisterPlugin(new PECGenJetMETReader);
    
    
    // Read original jets and MET
    PECJetMETReader *jetmetReader = new PECJetMETReader("OrigJetMET");
    jetmetReader->SetSelection(0., 5.);
    jetmetReader->ReadRawMET();
    jetmetReader->ConfigureLeptonCleaning("");  // Disabled
    jetmetReader->SetGenJetReader();  // Default one
    jetmetReader->SetApplyJetID(false);
    manager.RegisterPlugin(jetmetReader);
    
    
    // Corrections to be applied to jets and also to be propagated to MET. Although original
    //jets in simulation already have up-to-date corrections, they will be reapplied in order
    //to have a consistent impact on MET from the stochastic JER smearing. The random-number
    //seed for the smearing is fixed for the sake of reproducibility.
    JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
    
    if (optionsMap.count("add-jec"))
        jetCorrFull->SetJEC({jecVersion + "_MC_L1FastJet_AK4PFchs.txt",
          jecVersion + "_MC_L2Relative_AK4PFchs.txt",
          jecVersion + "_MC_L3Absolute_AK4PFchs.txt",
          optionsMap["add-jec"].as<string>()});
    else
        jetCorrFull->SetJEC({jecVersion + "_MC_L1FastJet_AK4PFchs.txt",
          jecVersion + "_MC_L2Relative_AK4PFchs.txt",
          jecVersion + "_MC_L3Absolute_AK4PFchs.txt"});

    jetCorrFull->SetJER("Spring16_25nsV10_MC_SF_AK4PFchs.txt",
      "Spring16_25nsV10_MC_PtResolution_AK4PFchs.txt", 4913);
    
    if (systType == "JEC")
        jetCorrFull->SetJECUncertainty(jecVersion + "_MC_Uncertainty_AK4PFchs.txt");
    
    manager.RegisterService(jetCorrFull);
    
    // L1 corrections to be used in T1 MET corrections
    JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
    jetCorrL1->SetJEC({jecVersion + "_MC_L1RC_AK4PFchs.txt"});
    manager.RegisterService(jetCorrL1);
    
    
    // Recorrect jets and apply T1 MET corrections to raw MET
    JetMETUpdate *jetmetUpdater = new JetMETUpdate;
    jetmetUpdater->SetJetCorrection("JetCorrFull");
    jetmetUpdater->SetJetCorrectionForMET("JetCorrFull", "JetCorrL1", "", "");
    jetmetUpdater->UseRawMET();
    manager.RegisterPlugin(jetmetUpdater);
    
    
    // Event selection
    manager.RegisterPlugin(new TriggerBin({400.}));
    
    if (optionsMap.count("wide"))
        manager.RegisterPlugin(new FirstJetFilter(0., 2.4));
    else
        manager.RegisterPlugin(new FirstJetFilter(0., 1.3));
    
    manager.RegisterPlugin(new GenMatchFilter(0.2, 0.5));
    
    
    std::vector<double> ptLeadBinning;
    
    for (double pt = 180.; pt < 2000. + 1.; pt += 5.)
        ptLeadBinning.emplace_back(pt);
    
    
    for (auto const &jetPtCut: jetPtCuts)
    {
        string const ptCutText(to_string(jetPtCut));
        
        RecoilBuilder *recoilBuilder = new RecoilBuilder("RecoilBuilderPt"s + ptCutText, jetPtCut);
        recoilBuilder->SetBalanceSelection(0.6, 0.3);
        manager.RegisterPlugin(recoilBuilder, {"GenMatchFilter"});
        
        manager.RegisterPlugin(new JetIDFilter("JetIDFilterPt"s + ptCutText, jetPtCut));
        
        if (dataGroup == DatasetGroup::Data)
        {
            BalanceHists *balanceHists = new BalanceHists("BalanceHistsPt"s + ptCutText);
            balanceHists->SetRecoilBuilderName(recoilBuilder->GetName());
            balanceHists->SetBinningPtLead(ptLeadBinning);
            manager.RegisterPlugin(balanceHists);
        }
        else
        {
            BalanceVars *balanceVars = new BalanceVars("BalanceVarsPt"s + ptCutText);
            balanceVars->SetRecoilBuilderName(recoilBuilder->GetName());
            manager.RegisterPlugin(balanceVars);
        }
        
        manager.RegisterPlugin(new PileUpVars("PileUpVarsPt"s + ptCutText));
    }
    
    
    // Process the datasets
    manager.Process(16);
    
    std::cout << '\n';
    manager.PrintSummary();
    
    
    return EXIT_SUCCESS;
}
