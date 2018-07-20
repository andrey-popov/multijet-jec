#include <BalanceFilter.hpp>
#include <BalanceHists.hpp>
#include <BalanceVars.hpp>
#include <DumpEventID.hpp>
#include <EtaPhiFilter.hpp>
#include <FirstJetFilter.hpp>
#include <GenMatchFilter.hpp>
#include <JERCJetMETReader.hpp>
#include <JetIDFilter.hpp>
#include <LeadJetTriggerFilter.hpp>
#include <PileUpVars.hpp>
#include <RecoilBuilder.hpp>

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
#include <mensura/PECReader/PECPileUpReader.hpp>
#include <mensura/PECReader/PECTriggerObjectReader.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <list>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>


using namespace std;
namespace po = boost::program_options;


enum class DatasetGroup
{
    Data,
    MC
};


/// Supported systematic uncertainties
enum class SystType
{
    None,   ///< No variation
    L1Res,  ///< Combined uncertainty in L1Res
    L2Res,  ///< Combined uncertainty in L2Res
    JER     ///< Variation due to JER
};


std::string systTypeToString(SystType systType)
{
    switch (systType)
    {
        case SystType::None:
            return "None";
        
        case SystType::L1Res:
            return "L1Res";
        
        case SystType::L2Res:
            return "L2Res";
        
        case SystType::JER:
            return "JER";
    }
    
    // This should never happen
    throw std::runtime_error("Unhandled type of systematic variation");
}


int main(int argc, char **argv)
{
    // Parse arguments
    po::options_description options("Allowed options");
    options.add_options()
      ("help,h", "Prints help message")
      ("datasetGroup", po::value<string>(), "Dataset group (required)")
      ("syst,s", po::value<string>(), "Systematic shift")
      ("era,e", po::value<string>(), "Data-taking era")
      // ("jec-version", po::value<string>(), "Optional explicit JEC version")
      ("l3-res", "Enables L3 residual corrections")
      ("no-res", "Disables all residual corrections")
      ("wide", "Loosen selection to |eta(j1)| < 2.4")
      ("output", po::value<string>()->default_value("output"),
        "Name for output directory");
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
    
    if (optionsMap.count("l3-res") and optionsMap.count("no-res"))
    {
        cerr << "Options \"--l3-res\" and \"--no-res\" conflict with each other.\n";
        return EXIT_FAILURE;
    }
    
    
    string dataGroupText(optionsMap["datasetGroup"].as<string>());
    DatasetGroup dataGroup;
    
    if (dataGroupText == "data")
        dataGroup = DatasetGroup::Data;
    else if (dataGroupText == "mc" or dataGroupText == "sim")
        dataGroup = DatasetGroup::MC;
    else
    {
        cerr << "Cannot recognize dataset group \"" << dataGroupText << "\".\n";
        return EXIT_FAILURE;
    }
    
    
    // Parse requested systematic uncertainty
    SystType systType(SystType::None);
    SystService::VarDirection systDirection = SystService::VarDirection::Undefined;
    
    if (optionsMap.count("syst"))
    {
        string systArg(optionsMap["syst"].as<string>());
        boost::to_lower(systArg);
        
        std::regex systRegex("(l1res|l2res|jer)[-_]?(up|down)", std::regex::extended);
        std::smatch matchResult;
        
        if (not std::regex_match(systArg, matchResult, systRegex))
        {
            cerr << "Cannot recognize systematic variation \"" << systArg << "\".\n";
            return EXIT_FAILURE;
        }
        else
        {
            if (matchResult[1] == "l1res")
                systType = SystType::L1Res;
            else if (matchResult[1] == "l2res")
                systType = SystType::L2Res;
            else if (matchResult[1] == "jer")
                systType = SystType::JER;
            
            if (matchResult[2] == "up")
                systDirection = SystService::VarDirection::Up;
            else if (matchResult[2] == "down")
                systDirection = SystService::VarDirection::Down;
        }
    }
    
    
    // Parse data-taking era
    string dataEra("None");
    
    if (optionsMap.count("era"))
    {
        dataEra = optionsMap["era"].as<string>();
        
        if (boost::starts_with(dataEra, "Run"))
            dataEra = dataEra.substr(3);
        
        
        set<string> allowedEras({
          "2016All", "2016BCD", "2016EFearly", "2016FlateG6H",
          "2017All", "2017B", "2017C", "2017D", "2017E", "2017F"});
        
        if (allowedEras.count(dataEra) == 0)
        {
            cerr << "Unsupported data-taking era \"" << dataEra << "\".\n";
            return EXIT_FAILURE;
        }
    }    
    
    
    // Input datasets
    list<Dataset> datasets;
    DatasetBuilder datasetBuilder("/gridgroup/cms/popov/Analyses/JetMET/"
      "2018.02.05_Grid-campaign-94X/Results/samples_v1.json");
    
    if (dataGroup == DatasetGroup::Data)
    {
        if (dataEra == "2016BCD")
            datasets = datasetBuilder({"JetHT-Run2016B_Ykc", "JetHT-Run2016C_gvU",
              "JetHT-Run2016D_cgp"});
        else if (dataEra == "2016EFearly")
            datasets = datasetBuilder({"JetHT-Run2016E_FVw", "JetHT-Run2016F_JAZ"});
        else if (dataEra == "2016FlateGH")
            datasets = datasetBuilder({"JetHT-Run2016F_JAZ", "JetHT-Run2016G_fJQ",
              "JetHT-Run2016H_xOF"});
        else if (dataEra == "2016All")
            datasets = datasetBuilder({"JetHT-Run2016B_Ykc", "JetHT-Run2016C_gvU",
              "JetHT-Run2016D_cgp", "JetHT-Run2016E_FVw", "JetHT-Run2016F_JAZ",
              "JetHT-Run2016G_fJQ", "JetHT-Run2016H_xOF"});
        
        else if (dataEra == "2017B")
            datasets = datasetBuilder({"JetHT-Run2017B_RJp"});
        else if (dataEra == "2017C")
            datasets = datasetBuilder({"JetHT-Run2017C_yPY"});
        else if (dataEra == "2017D")
            datasets = datasetBuilder({"JetHT-Run2017D_lFY"});
        else if (dataEra == "2017E")
            datasets = datasetBuilder({"JetHT-Run2017E_wbU"});
        else if (dataEra == "2017F")
            datasets = datasetBuilder({"JetHT-Run2017F_toh"});
        else if (dataEra == "2017All")
            datasets = datasetBuilder({"JetHT-Run2017B_RJp", "JetHT-Run2017C_yPY",
              "JetHT-Run2017D_lFY", "JetHT-Run2017E_wbU", "JetHT-Run2017F_toh"});
    }
    else
        datasets = datasetBuilder({"QCD-Ht-100-200-mg_mtD", "QCD-Ht-200-300-mg_Trm",
          "QCD-Ht-300-500-mg_nlt", "QCD-Ht-500-700-mg_lXD", "QCD-Ht-700-1000-mg_pOy",
          "QCD-Ht-1000-1500-mg_peK", "QCD-Ht-1500-2000-mg_sOQ", "QCD-Ht-2000-inf-mg_Xel"});
    
    
    // Add an additional locations to seach for data files
    char const *installPath = getenv("MULTIJET_JEC_INSTALL");
    
    if (not installPath)
    {
        cerr << "Mandatory environmental variable MULTIJET_JEC_INSTALL is not defined.\n";
        return EXIT_FAILURE;
    }
    
    FileInPath::AddLocation(string(installPath) + "/data/");
    FileInPath::AddLocation(string(installPath) + "/config/");
    FileInPath::AddLocation("/gridgroup/cms/popov/Analyses/JetMET/JERC/");
    
    
    // Construct the run manager
    RunManager manager(datasets.begin(), datasets.end());
    
    
    // Register services and plugins
    ostringstream outputNameStream;
    outputNameStream << optionsMap["output"].as<string>() << "/" <<
      ((dataGroup == DatasetGroup::Data) ? dataEra : "sim");
    
    if (systType != SystType::None)
        outputNameStream << "_" << systTypeToString(systType) << "_" <<
          ((systDirection == SystService::VarDirection::Up) ? "up" : "down");
    
    outputNameStream << "/%";
    
    manager.RegisterService(new TFileService(outputNameStream.str()));
    
    manager.RegisterPlugin(new PECInputData);
    manager.RegisterPlugin(new PECPileUpReader);
    
    
    // Jet corrections
    if (dataGroup == DatasetGroup::Data)
    {
        // Read original jets and MET, which have outdated corrections
        JERCJetMETReader *jetmetReader = new JERCJetMETReader("OrigJetMET");
        jetmetReader->SetSelection(0., 5.);
        jetmetReader->SetSelection([](Jet const &j){
          return std::abs(j.Eta()) < 2.650 or std::abs(j.Eta()) > 3.139;});
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->SetApplyJetID(false);
        manager.RegisterPlugin(jetmetReader);
        
        
        // Corrections to be applied to jets. The full correction will also be propagated into
        //missing pt.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
        
        for (auto jetCorr: {jetCorrFull, jetCorrL1})
        {
            jetCorr->RegisterIOV("2017B", 297020, 299329);
            jetCorr->RegisterIOV("2017C", 299337, 302029);
            jetCorr->RegisterIOV("2017D", 302030, 303434);
            jetCorr->RegisterIOV("2017E", 303435, 304826);
            jetCorr->RegisterIOV("2017F", 304911, 306462);
        }
        
        for (string const &era: {"2017B", "2017C", "2017D", "2017E", "2017F"})
        {
            string const jecVersion = "Fall17_17Nov" + era + "_V11";
            
            vector<string> jecLevels{jecVersion + "_DATA_L1FastJet_AK4PFchs.txt",
              jecVersion + "_DATA_L2Relative_AK4PFchs.txt",
              jecVersion + "_DATA_L3Absolute_AK4PFchs.txt"};
            
            if (not optionsMap.count("no-res"))
            {
                if (optionsMap.count("l3-res"))
                    jecLevels.emplace_back(jecVersion + "_DATA_L2L3Residual_AK4PFchs.txt");
                else
                {
                    if (systType == SystType::JER)
                    {
                        // Modified L2Res derived with varied JER. Taken from [1].
                        //[1] https://indico.cern.ch/event/734120/#4-l2res-for-ak4pfchs-2017-bcde
                        if (systDirection == SystService::VarDirection::Up)
                            jecLevels.push_back("L2Res_JER-up_180604_Fall17_17Nov2017_MPF_LOGLIN_L2Residual_pythia8_AK4PFchs.txt");
                        else
                            jecLevels.push_back("L2Res_JER-down_180604_Fall17_17Nov2017_MPF_LOGLIN_L2Residual_pythia8_AK4PFchs.txt");
                    }
                    else
                    {
                        // Nominal L2Res
                        jecLevels.emplace_back(jecVersion + "_DATA_L2Residual_AK4PFchs.txt");
                    }
                }
            }
            
            
            jetCorrFull->SetJEC(era, jecLevels);
            jetCorrL1->SetJEC(era, {jecVersion + "_DATA_L1RC_AK4PFchs.txt"});
        }
        
        manager.RegisterService(jetCorrFull);
        manager.RegisterService(jetCorrL1);
        
        
        // Recorrect jets and apply T1 MET corrections to raw MET
        JetMETUpdate *jetmetUpdater = new JetMETUpdate;
        jetmetUpdater->SetJetCorrection("JetCorrFull");
        jetmetUpdater->SetJetCorrectionForMET("JetCorrFull", "JetCorrL1", "", "");
        jetmetUpdater->UseRawMET();
        manager.RegisterPlugin(jetmetUpdater);
    }
    else
    {
        manager.RegisterService(new SystService(
          (systType == SystType::None or systType == SystType::JER) ?
            systTypeToString(systType) : "JEC"s,
          systDirection));
        
        manager.RegisterPlugin(new PECGenJetMETReader);
        
        
        // Read original jets and MET
        JERCJetMETReader *jetmetReader = new JERCJetMETReader("OrigJetMET");
        jetmetReader->SetSelection(0., 5.);
        jetmetReader->SetSelection([](Jet const &j){
          return std::abs(j.Eta()) < 2.650 or std::abs(j.Eta()) > 3.139;});
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->SetGenJetReader();  // Default one
        jetmetReader->SetApplyJetID(false);
        manager.RegisterPlugin(jetmetReader);
        
        
        string const jecVersion("Fall17_17Nov2017_V11");
        
        // Corrections to be applied to jets and also to be propagated to MET. Although original
        //jets in simulation already have up-to-date corrections, they will be reapplied in order
        //to have a consistent impact on MET from the stochastic JER smearing. The random-number
        //seed for the smearing is fixed for the sake of reproducibility.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        jetCorrFull->SetJEC({jecVersion + "_MC_L1FastJet_AK4PFchs.txt",
          jecVersion + "_MC_L2Relative_AK4PFchs.txt",
          jecVersion + "_MC_L3Absolute_AK4PFchs.txt"});
        jetCorrFull->SetJER("Summer16_25nsV1_MC_SF_AK4PFchs.txt",
          "Summer16_25nsV1_MC_PtResolution_AK4PFchs.txt");
        
        if (systType == SystType::L1Res)
            jetCorrFull->SetJECUncertainty(jecVersion + "_MC_UncertaintySources_AK4PFchs.txt",
              {"PileUpPtBB", "PileUpPtEC1", "PileUpPtEC2", "PileUpPtHF", "PileUpDataMC"});
        else if (systType == SystType::L2Res)
            jetCorrFull->SetJECUncertainty(jecVersion + "_MC_UncertaintySources_AK4PFchs.txt",
              {"RelativePtBB", "RelativePtEC1", "RelativePtEC2", "RelativePtHF",
               "RelativeBal", "RelativeSample", "RelativeFSR",
               "RelativeStatFSR", "RelativeStatEC", "RelativeStatHF"});
        
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
    }
    
    
    if (optionsMap.count("wide"))
        manager.RegisterPlugin(new FirstJetFilter(150., 2.4));
    else
        manager.RegisterPlugin(new FirstJetFilter(150., 1.3));
    
    manager.RegisterPlugin(new JetIDFilter("JetIDFilter", 15.));
    
    if (dataGroup == DatasetGroup::Data)
        manager.RegisterPlugin(new EtaPhiFilter("EtaPhiFilter", 15., "BadRegions_2017.root"));
    
    if (dataGroup == DatasetGroup::MC)
        manager.RegisterPlugin(new GenMatchFilter(0.2, 0.5));
    
    RecoilBuilder *recoilBuilder = new RecoilBuilder("RecoilBuilder", 30.);
    recoilBuilder->SetBalanceSelection(0.6, 0.3);
    manager.RegisterPlugin(recoilBuilder);
    
    // Remove strongly imbalanced events in the high-pt region. This is a temporary solution to the
    //problem described in [1].
    //[1] https://indico.cern.ch/event/737609/#17-modified-typei-met-study-wi
    BalanceFilter *balanceFilter = new BalanceFilter(0.5, 1.5);
    balanceFilter->SetMinPtLead(1000.);
    manager.RegisterPlugin(balanceFilter);
    
    
    manager.RegisterPlugin(new PECTriggerObjectReader);
    
    for (string const &trigger:
      {"PFJet140", "PFJet200", "PFJet260", "PFJet320", "PFJet400", "PFJet450", "PFJet500"})
    {
        manager.RegisterPlugin(new LeadJetTriggerFilter("TriggerFilter"s + trigger, trigger,
          "triggerBins.json", (dataGroup == DatasetGroup::Data)), {"RecoilBuilder"});
        
        BalanceVars *balanceVars = new BalanceVars("BalanceVars"s + trigger);
        balanceVars->SetTreeName(trigger + "/BalanceVars");
        manager.RegisterPlugin(balanceVars, {"TriggerFilter"s + trigger});
        
        PileUpVars *puVars = new PileUpVars("PileUpVars"s + trigger);
        puVars->SetTreeName(trigger + "/PileUpVars");
        manager.RegisterPlugin(puVars);
        
        if (dataGroup == DatasetGroup::Data)
        {
            DumpEventID *eventID = new DumpEventID("EventID"s + trigger);
            eventID->SetTreeName(trigger + "/EventID");
            manager.RegisterPlugin(eventID);
            
            BalanceHists *balanceHists = new BalanceHists("BalanceHists"s + trigger, 10.);
            balanceHists->SetDirectoryName(trigger);
            manager.RegisterPlugin(balanceHists);
        }
    }
    
    
    // Process the datasets
    manager.Process(16);
    
    std::cout << '\n';
    manager.PrintSummary();
    
    
    return EXIT_SUCCESS;
}
