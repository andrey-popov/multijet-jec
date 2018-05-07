#include <BalanceFilter.hpp>
#include <BalanceHists.hpp>
#include <BalanceVars.hpp>
#include <DumpEventID.hpp>
#include <EtaPhiFilter.hpp>
#include <FirstJetFilter.hpp>
#include <GenMatchFilter.hpp>
#include <JetIDFilter.hpp>
#include <LeadJetTriggerFilter.hpp>
#include <PileUpVars.hpp>
#include <RecoilBuilder.hpp>
#include <RunFilter.hpp>

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
    Run2016FlateGH
};


int main(int argc, char **argv)
{
    // Parse arguments
    po::options_description options("Allowed options");
    options.add_options()
      ("help,h", "Prints help message")
      ("datasetGroup", po::value<string>(), "Dataset group (required)")
      ("syst,s", po::value<string>(), "Systematic shift")
      ("era,e", po::value<string>(), "Data-taking era")
      ("jec-version", po::value<string>(), "Optional explicit JEC version")
      ("l3-res", "Enables L3 residual corrections")
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
    
    
    // Parse data-taking era
    Era dataEra = Era::All;
    string dataEraText;
    
    if (optionsMap.count("era"))
    {
        dataEraText = optionsMap["era"].as<string>();
        
        if (dataEraText == "Run2016BCD")
            dataEra = Era::Run2016BCD;
        else if (dataEraText == "Run2016EFearly")
            dataEra = Era::Run2016EFearly;
        else if (dataEraText == "Run2016FlateGH")
            dataEra = Era::Run2016FlateGH;
        else
        {
            cerr << "Cannot recognize data-taking era \"" << dataEraText << "\".\n";
            return EXIT_FAILURE;
        }
    }
    
    if (dataGroup == DatasetGroup::Data and dataEra == Era::All)
    {
        cerr << "Requested to run over full data-taking period, but no residual JEC are "
          "available for it.\n";
        return EXIT_FAILURE;
    }
    
    
    
    // Input datasets
    list<Dataset> datasets;
    DatasetBuilder datasetBuilder("/gridgroup/cms/popov/Analyses/JetMET/"
      "2017.10.19_Grid-campaign-07Aug17/Results/samples_v1.json");
    
    if (dataGroup == DatasetGroup::Data)
    {
        switch (dataEra)
        {
            case Era::Run2016BCD:
                datasets = datasetBuilder({"JetHT-Run2016B_Ykc", "JetHT-Run2016C_gvU",
                  "JetHT-Run2016D_cgp"});
                break;
            
            case Era::Run2016EFearly:
                datasets = datasetBuilder({"JetHT-Run2016E_FVw", "JetHT-Run2016F_JAZ"});
                break;
            
            case Era::Run2016FlateGH:
                datasets = datasetBuilder({"JetHT-Run2016F_JAZ", "JetHT-Run2016G_fJQ",
                  "JetHT-Run2016H_xOF"});
                break;
            
            default:
                break;
        }
    }
    else
        datasets = datasetBuilder({"QCD-Ht-100-200-mg_fRs", "QCD-Ht-200-300-mg_all",
          "QCD-Ht-300-500-mg_all", "QCD-Ht-500-700-mg_all", "QCD-Ht-700-1000-mg_all",
          "QCD-Ht-1000-1500-mg_all", "QCD-Ht-1500-2000-mg_all", "QCD-Ht-2000-inf-mg_all"});
    
    
    // Add an additional locations to seach for data files
    char const *installPath = getenv("MULTIJET_JEC_INSTALL");
    
    if (not installPath)
    {
        cerr << "Mandatory environmental variable MULTIJET_JEC_INSTALL is not defined.\n";
        return EXIT_FAILURE;
    }
    
    FileInPath::AddLocation(string(installPath) + "/config/");
    FileInPath::AddLocation("/gridgroup/cms/popov/Analyses/JetMET/JERC/");
    
    
    // Construct the run manager
    RunManager manager(datasets.begin(), datasets.end());
    
    
    // Register services and plugins
    ostringstream outputNameStream;
    outputNameStream << "output/" << ((dataGroup == DatasetGroup::Data) ? dataEraText : "sim");
    
    if (systType != "None")
        outputNameStream << "_" << systType << "_" <<
          ((systDirection == SystService::VarDirection::Up) ? "up" : "down");
    
    outputNameStream << "/%";
    
    manager.RegisterService(new TFileService(outputNameStream.str()));
    
    manager.RegisterPlugin(new PECInputData);
    manager.RegisterPlugin(new PECPileUpReader);
    
    if (dataGroup == DatasetGroup::Data)
    {
        if (dataEra == Era::Run2016EFearly)
            manager.RegisterPlugin(new RunFilter(RunFilter::Mode::Less, 278802));
        else if (dataEra == Era::Run2016FlateGH)
            manager.RegisterPlugin(new RunFilter(RunFilter::Mode::GreaterEq, 278802));
    }
    
    
    // Determine JEC version
    string jecVersion;
    
    if (optionsMap.count("jec-version"))
    {
        // If a JEC version is given explicitly, use it regardless of the data era
        jecVersion = optionsMap["jec-version"].as<string>();
    }
    else
    {
        if (dataGroup == DatasetGroup::Data)
        {
            jecVersion = "Summer16_07Aug2017";
            
            switch (dataEra)
            {
                case Era::Run2016BCD:
                    jecVersion += "BCD";
                    break;
                
                case Era::Run2016EFearly:
                    jecVersion += "EF";
                    break;
                
                case Era::Run2016FlateGH:
                    jecVersion += "GH";
                    break;
            }
            
            jecVersion += "_V10";
        }
        else
            jecVersion = "Summer16_07Aug2017_V10";
    }
    
    
    // Jet corrections
    if (dataGroup == DatasetGroup::Data)
    {
        // Read original jets and MET, which have outdated corrections
        PECJetMETReader *jetmetReader = new PECJetMETReader("OrigJetMET");
        jetmetReader->SetSelection(0., 5.);
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->ReadRawMET();
        jetmetReader->SetApplyJetID(false);
        manager.RegisterPlugin(jetmetReader);
        
        
        // Corrections to be applied to jets. They will also be propagated to MET.
        string residualsType = (optionsMap.count("l3-res")) ? "L2L3Residual" : "L2Residual";
        
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        jetCorrFull->SetJEC({jecVersion + "_DATA_L1FastJet_AK4PFchs.txt",
          jecVersion + "_DATA_L2Relative_AK4PFchs.txt",
          jecVersion + "_DATA_L3Absolute_AK4PFchs.txt",
          jecVersion + "_DATA_" + residualsType + "_AK4PFchs.txt"});
        manager.RegisterService(jetCorrFull);
        
        // L1 corrections to be used in T1 MET corrections
        JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
        jetCorrL1->SetJEC({jecVersion + "_DATA_L1RC_AK4PFchs.txt"});
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
        jetCorrFull->SetJEC({jecVersion + "_MC_L1FastJet_AK4PFchs.txt",
          jecVersion + "_MC_L2Relative_AK4PFchs.txt",
          jecVersion + "_MC_L3Absolute_AK4PFchs.txt"});
        jetCorrFull->SetJER("Summer16_25nsV1_MC_SF_AK4PFchs.txt",
          "Summer16_25nsV1_MC_PtResolution_AK4PFchs.txt");
        
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
    }
    
    
    if (optionsMap.count("wide"))
        manager.RegisterPlugin(new FirstJetFilter(150., 2.4));
    else
        manager.RegisterPlugin(new FirstJetFilter(150., 1.3));
    
    manager.RegisterPlugin(new JetIDFilter("JetIDFilter", 15.));
    
    if (dataGroup == DatasetGroup::Data and dataEra == Era::Run2016BCD)
    {
        EtaPhiFilter *etaPhiFilter = new EtaPhiFilter(15.);
        
        // Definition from 06.12.2017
        etaPhiFilter->AddRegion(272007, 275376, -2.250, -1.930, 2.200, 2.500);
        etaPhiFilter->AddRegion(275657, 276283, -3.489, -3.139, 2.237, 2.475);
        etaPhiFilter->AddRegion(276315, 276811, -3.600, -3.139, 2.237, 2.475);
        
        manager.RegisterPlugin(etaPhiFilter);
    }
    
    if (dataGroup == DatasetGroup::MC)
        manager.RegisterPlugin(new GenMatchFilter(0.2, 0.5));
    
    RecoilBuilder *recoilBuilder = new RecoilBuilder("RecoilBuilder", 30.);
    recoilBuilder->SetBalanceSelection(0.6, 0.3);
    manager.RegisterPlugin(recoilBuilder);
    
    // Remove strongly imbalanced events in the high-pt region. This is a temporary solution to the
    //problem described in [1].
    //[1] https://indico.cern.ch/event/720429/#7-unhealthy-high-pt-electrons
    BalanceFilter *balanceFilter = new BalanceFilter(0.5, 1.5);
    balanceFilter->SetMinPtLead(1000.);
    manager.RegisterPlugin(balanceFilter);
    
    
    manager.RegisterPlugin(new PECTriggerObjectReader);
    
    for (string const &trigger:
      {"PFJet140", "PFJet200", "PFJet260", "PFJet320", "PFJet400", "PFJet450"})
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
