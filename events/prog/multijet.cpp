/**
 * \file multijet.cpp
 *
 * The main application. Runs over input files with events, applies the event selection, fills trees
 * and histograms for subsequent high-level analysis.
 *
 * Command-line arguments (as opposed to options) define input files. They can be interpreted in two
 * different ways, see the documentation for \ref BuildDatasets.
 */

#include <AngularFilter.hpp>
#include <BalanceCalc.hpp>
#include <BalanceFilter.hpp>
#include <BalanceHists.hpp>
#include <BalanceVars.hpp>
#include <DumpEventID.hpp>
#include <EtaPhiFilter.hpp>
#include <FirstJetFilter.hpp>
#include <GenMatchFilter.hpp>
#include <GenWeights.hpp>
#include <JERCJetMETReader.hpp>
#include <JERCJetMETUpdate.hpp>
#include <JetIDFilter.hpp>
#include <LeadJetTriggerFilter.hpp>
#include <MPIMatchFilter.hpp>
#include <PeriodWeights.hpp>
#include <PileUpVars.hpp>

#include <mensura/Config.hpp>
#include <mensura/Dataset.hpp>
#include <mensura/DatasetBuilder.hpp>
#include <mensura/FileInPath.hpp>
#include <mensura/JetCorrectorService.hpp>
#include <mensura/JetFilter.hpp>
#include <mensura/RunManager.hpp>
#include <mensura/SystService.hpp>
#include <mensura/TFileService.hpp>

#include <mensura/PECReader/PECGenJetMETReader.hpp>
#include <mensura/PECReader/PECGeneratorReader.hpp>
#include <mensura/PECReader/PECGenParticleReader.hpp>
#include <mensura/PECReader/PECInputData.hpp>
#include <mensura/PECReader/PECPileUpReader.hpp>
#include <mensura/PECReader/PECTriggerObjectReader.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <stdexcept>
#include <regex>


namespace fs = std::filesystem;
namespace po = boost::program_options;
using namespace std;


/// Supported systematic uncertainties
enum class SystType
{
    None,   ///< No variation
    L1Res,  ///< Combined uncertainty in L1Res
    L2Res,  ///< Combined uncertainty in L2Res
    JER     ///< Variation due to JER
};


/**
 * \brief Constructs input data sets
 *
 * \param[in] inputs  A non-empty vector of inputs. Its interpetation depends on the number of
 *     elements. If there is only one, it is interpreted as the label identifying a sample group
 *     defined in the configuration file. Otherwise the first element is interpreted as the data set
 *     ID and all the following ones as paths to input files. Relative paths are resolved with
 *     respect to the base directory of the underlying DatasetBuilder.
 * \param[in] config  Object that provides an access to the configuration.
 */
std::list<Dataset> BuildDatasets(std::vector<std::string> const &inputs, Config const &config);


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
      ("sample_def", po::value<vector<string>>(), "Definition of input samples (required)")
      ("config,c", po::value<string>()->default_value("main.json"), "Configuration file")
      ("syst,s", po::value<string>(), "Systematic shift")
      ("l3-res", "Enables L3 residual corrections")
      ("wide", "Loosen selection to |eta(j1)| < 2.4")
      ("output", po::value<string>()->default_value("output"),
        "Name for output directory")
      ("threads,t", po::value<int>()->default_value(1), "Number of threads to run in parallel");
    
    po::positional_options_description positionalOptions;
    positionalOptions.add("sample_def", -1);
    
    po::variables_map optionsMap;
    
    po::store(
      po::command_line_parser(argc, argv).options(options).positional(positionalOptions).run(),
      optionsMap);
    po::notify(optionsMap);
    
    if (optionsMap.count("help"))
    {
        cerr << "Produces tuples with observables for the multijet method.\n";
        cerr << "Usage: multijet sample_group [options]\n";
        cerr << options << endl;
        return EXIT_FAILURE;
    }
    
    if (not optionsMap.count("sample_def"))
    {
        cerr << "Required argument sample_def is missing.\n";
        return EXIT_FAILURE;
    }
    

    // Load the main configuration and include additional locations to search for files
    char const *installPath = getenv("MULTIJET_JEC_INSTALL");
    
    if (not installPath)
    {
        cerr << "Mandatory environmental variable MULTIJET_JEC_INSTALL is not defined.\n";
        return EXIT_FAILURE;
    }
    
    FileInPath::AddLocation(string(installPath) + "/config/");

    Config config(optionsMap["config"].as<string>());

    auto const &addLocationsNode = config.Get({"add_locations"});

    for (unsigned i = 0; i < addLocationsNode.size(); ++i)
        FileInPath::AddLocation(addLocationsNode[i].asString());
    
    
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
    
    
    // Input datasets
    std::list<Dataset> const datasets = BuildDatasets(
      optionsMap["sample_def"].as<std::vector<std::string>>(), config);

    // Use the first data set to determine whether real data or simulation is being processed. All
    // other data sets must be the same.
    bool const isSim = datasets.front().IsMC();
    
    // Construct the run manager
    RunManager manager(datasets.begin(), datasets.end());
    
    
    // Register services and plugins
    ostringstream outputNameStream;
    outputNameStream << optionsMap["output"].as<string>();
    
    if (systType != SystType::None)
        outputNameStream << "_" << systTypeToString(systType) << "_" <<
          ((systDirection == SystService::VarDirection::Up) ? "up" : "down");
    
    outputNameStream << "/%";
    
    manager.RegisterService(new TFileService(outputNameStream.str()));
    
    manager.RegisterPlugin(new PECInputData);
    manager.RegisterPlugin(new PECPileUpReader);
    
    
    // Jet corrections
    if (not isSim)
    {
        // Read original jets and MET, which have outdated corrections
        JERCJetMETReader *jetmetReader = new JERCJetMETReader("OrigJetMET");
        jetmetReader->SetSelection(0., 5.);
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->SetApplyJetID(false);
        manager.RegisterPlugin(jetmetReader);
        
        
        // Corrections to be applied to jets. The full correction will also be propagated into
        //missing pt.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
        
        for (auto jetCorr: {jetCorrFull, jetCorrL1})
        {
            // Periods for jet corrections are not aligned perfectly with data-taking eras: the
            //period "2016GH" includes few last runs from era 2016F
            jetCorr->RegisterIOV("2016BCD", 272007, 276811);
            jetCorr->RegisterIOV("2016EF", 276831, 278801);
            jetCorr->RegisterIOV("2016GH", 278802, 284044);
        }
        
        for (string const &period: {"BCD", "EF", "GH"})
        {
            string const jecVersion = "Summer16_07Aug2017" + period + "_V20";
            
            vector<string> jecLevels{jecVersion + "_DATA_L1FastJet_AK4PFchs.txt",
              jecVersion + "_DATA_L2Relative_AK4PFchs.txt",
              jecVersion + "_DATA_L3Absolute_AK4PFchs.txt"};
            
            if (not optionsMap.count("no-res"))
            {
                if (optionsMap.count("l3-res"))
                    jecLevels.emplace_back(jecVersion + "_DATA_L2L3Residual_AK4PFchs.txt");
                else
                {
                    jecLevels.emplace_back(jecVersion + "_DATA_L2Residual_AK4PFchs.txt");
                    
                    if (systType == SystType::JER)
                    {
                        // Add closure-style L2Res corrections obtained with varied JER. Taken
                        //from [1], "kFSR_Fit" versions.  Use the variant obtained with the pt
                        //balance because the variations for MPF are one-sided in some cases.
                        //[1] https://indico.cern.ch/event/770859/#11-l2rec-closure-and-jer-updow
                        if (systDirection == SystService::VarDirection::Up)
                            jecLevels.emplace_back("Summer16_07Aug2017" + period +
                              "_V18_pT_LOGLIN_L2Residual_pythia8_AK4PFchs_JERUp.txt");
                        else
                            jecLevels.emplace_back("Summer16_07Aug2017" + period +
                              "_V18_pT_LOGLIN_L2Residual_pythia8_AK4PFchs_JERDown.txt");
                    }
                }
            }
            
            jetCorrFull->SetJEC("2016" + period, jecLevels);
            jetCorrL1->SetJEC("2016" + period, {jecVersion + "_DATA_L1RC_AK4PFchs.txt"});
        }
        
        manager.RegisterService(jetCorrFull);
        manager.RegisterService(jetCorrL1);
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
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->SetGenJetReader();  // Default one
        jetmetReader->SetApplyJetID(false);
        manager.RegisterPlugin(jetmetReader);
        
        
        string const jecVersion("Summer16_07Aug2017_V20");
        
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
    }
    
    
    // Recorrect jets and apply T1 MET corrections to raw MET
    JERCJetMETUpdate *jetmetUpdater = new JERCJetMETUpdate("JetCorrFull", "JetCorrL1");
    jetmetUpdater->SetT1Threshold(15., 20.);
    manager.RegisterPlugin(jetmetUpdater);
    
    
    if (optionsMap.count("wide"))
        manager.RegisterPlugin(new FirstJetFilter(150., 2.4));
    else
        manager.RegisterPlugin(new FirstJetFilter(150., 1.3));
    
    manager.RegisterPlugin(new JetIDFilter("JetIDFilter", 15.));
    
    if (not isSim)
    {
        EtaPhiFilter *etaPhiFilter = new EtaPhiFilter(15.);
        
        // Definition from 06.12.2017
        etaPhiFilter->AddRegion(272007, 275376, -2.250, -1.930, 2.200, 2.500);
        etaPhiFilter->AddRegion(275657, 276283, -3.489, -3.139, 2.237, 2.475);
        etaPhiFilter->AddRegion(276315, 276811, -3.600, -3.139, 2.237, 2.475);
        
        manager.RegisterPlugin(etaPhiFilter);
    }
    else
    {
        manager.RegisterPlugin(new PECGenParticleReader);
        manager.RegisterPlugin(new GenMatchFilter(0.2, 0.5));
        manager.RegisterPlugin(new MPIMatchFilter(0.4));

        auto *generatorReader = new PECGeneratorReader;
        generatorReader->RequestAltWeights();
        manager.RegisterPlugin(generatorReader);
    }
    
    // Set angular selection based on [1-3]
    //[1] https://indico.cern.ch/event/749862/#2-l3res-multijet-update
    //[2] https://indico.cern.ch/event/759977/#28-ideas-on-multijet
    AngularFilter *angularFilter = new AngularFilter;
    angularFilter->SetDPhi12Cut(2., 2.9);
    angularFilter->SetDPhi23Cut(0., 1.);
    manager.RegisterPlugin(angularFilter);
    
    manager.RegisterPlugin(new BalanceCalc(30., 33.));
    
    // Remove strongly imbalanced events in the high-pt region. This is a temporary solution to the
    //problem described in [1].
    //[1] https://indico.cern.ch/event/720429/#7-unhealthy-high-pt-electrons
    BalanceFilter *balanceFilter = new BalanceFilter(0.5, 1.5);
    balanceFilter->SetMinPtLead(1000.);
    manager.RegisterPlugin(balanceFilter);


    // Find requested trigger bins
    fs::path const triggerConfigPath = config.Get({"trigger_config"}).asString();
    Config triggerConfig(triggerConfigPath);
    std::vector<std::string> triggerNames;

    for (auto const &trigger: triggerConfig.Get().getMemberNames())
        triggerNames.emplace_back(trigger);
    
    
    manager.RegisterPlugin(new PECTriggerObjectReader);
    
    for (auto const &trigger: triggerNames)
    {
        manager.RegisterPlugin(new LeadJetTriggerFilter("TriggerFilter"s + trigger, trigger,
          triggerConfigPath, isSim), {"BalanceFilter"});
        
        BalanceVars *balanceVars = new BalanceVars("BalanceVars"s + trigger, 30.);
        balanceVars->SetTreeName(trigger + "/BalanceVars");
        manager.RegisterPlugin(balanceVars, {"TriggerFilter"s + trigger});
        
        PileUpVars *puVars = new PileUpVars("PileUpVars"s + trigger);
        puVars->SetTreeName(trigger + "/PileUpVars");
        manager.RegisterPlugin(puVars);
        
        if (isSim)
        {
            Weights *weights = new Weights("Weights" + trigger);
            weights->SetTreeName(trigger + "/Weights");
            weights->SetGeneratorReader("Generator");
            manager.RegisterPlugin(weights);

            PeriodWeights *periodWeights = new PeriodWeights("PeriodWeights" + trigger,
              config.Get({"period_weight_config"}).asString(), trigger);
            periodWeights->SetTreeName(trigger + "/PeriodWeights");
            manager.RegisterPlugin(periodWeights);
        }
        else
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
    manager.Process(optionsMap["threads"].as<int>());
    
    std::cout << '\n';
    manager.PrintSummary();
    
    
    return EXIT_SUCCESS;
}


std::list<Dataset> BuildDatasets(std::vector<std::string> const &inputs, Config const &config)
{
    std::list<Dataset> datasets;
    DatasetBuilder datasetBuilder(config.Get({"samples", "definition_file"}).asString());

    if (inputs.size() == 1)
    {
        // The only input is interpreted as the name of a sample group
        std::string const &sampleGroup = inputs[0];
        auto const &sampleGroupsNode = config.Get({"samples", "groups"});

        if (not sampleGroupsNode.isMember(sampleGroup))
        {
            cerr << "Unrecognized sample group \"" << sampleGroup << "\".\n";
            std::exit(EXIT_FAILURE);
        }

        auto const &sampleGroupNode = sampleGroupsNode[sampleGroup];

        for (unsigned i = 0; i < sampleGroupNode.size(); ++i)
        {
            std::string const datasetId = sampleGroupNode[i].asString();
            datasets.splice(datasets.end(), datasetBuilder(datasetId));
        }
    }
    else
    {
        // The first input is interpreted as a data set ID. All the rest are assumed to be paths
        // of input files.
        Dataset dataset = datasetBuilder.BuildEmpty(inputs[0]);

        for (unsigned i = 1; i < inputs.size(); ++i)
        {
            fs::path const path{inputs[i]};

            if (path.is_absolute())
                dataset.AddFile(path);
            else
            {
                // Relative paths are interpreted with respect to the base directory of the
                // DatasetBuilder
                dataset.AddFile(datasetBuilder.GetBaseDirectory() / path);
            }
        }

        datasets.emplace_back(std::move(dataset));
    }

    return datasets;
}

