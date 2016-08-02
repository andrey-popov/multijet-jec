#include <BalanceVars.hpp>
#include <DynamicPileUpWeight.hpp>
#include <DynamicTriggerFilter.hpp>
#include <FirstJetFilter.hpp>
#include <PileUpVars.hpp>
#include <RecoilBuilder.hpp>
#include <TriggerBin.hpp>

#include <mensura/core/Dataset.hpp>
#include <mensura/core/FileInPath.hpp>
#include <mensura/core/RunManager.hpp>
#include <mensura/core/SystService.hpp>

#include <mensura/extensions/JetCorrectorService.hpp>
#include <mensura/extensions/JetFilter.hpp>
#include <mensura/extensions/JetMETUpdate.hpp>
#include <mensura/extensions/PileUpWeight.hpp>
#include <mensura/extensions/TFileService.hpp>

#include <mensura/PECReader/PECInputData.hpp>
#include <mensura/PECReader/PECJetMETReader.hpp>
#include <mensura/PECReader/PECPileUpReader.hpp>

#include <boost/algorithm/string.hpp>

#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>
#include <regex>


using namespace std;


enum class DatasetGroup
{
    Data,
    MC
};


int main(int argc, char **argv)
{
    // Parse arguments
    if ((argc < 2 and argc > 4) or (argc == 2 and string(argv[1]) == "-h"))
    {
        cerr << "Usage: multijet dataset-group [jet-pt-cuts] [syst]\n";
        return EXIT_FAILURE;
    }
    
    
    string dataGroupText(argv[1]);
    DatasetGroup dataGroup;
    
    if (dataGroupText == "data")
        dataGroup = DatasetGroup::Data;
    else if (dataGroupText == "mc")
        dataGroup = DatasetGroup::MC;
    else
    {
        cerr << "Cannot recognize dataset group \"" << dataGroupText << "\".\n";
        return EXIT_FAILURE;
    }
    
    
    // Parse list of jet pt cuts used to construct the recoil. Their values are assumed to be
    //integer.
    list<unsigned> jetPtCuts;
    
    if (argc >= 3)
    {
        istringstream cutList(argv[2]);
        string cut;
        
        while (getline(cutList, cut, ','))
            jetPtCuts.emplace_back(stod(cut));
    }
    else
        jetPtCuts = {30};
    
    
    // Parse requested systematic uncertainty
    string systType("None");
    SystService::VarDirection systDirection = SystService::VarDirection::Undefined;
    
    if (argc >= 4)
    {
        string systArg(argv[3]);
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
    string const datasetsDir("/gridgroup/cms/popov/Analyses/JetMET/2016.05.21_Grid-campaign-80X/"
      "Results/");
    
    if (dataGroup == DatasetGroup::Data)
    {
        datasets.emplace_back(Dataset({Dataset::Process::ppData, Dataset::Process::pp13TeV},
          Dataset::Generator::Nature, Dataset::ShowerGenerator::Nature));
        datasets.back().AddFile(datasetsDir + "JetHT-Run2016B_b8aae01_xfJ_p*.root");
        datasets.back().AddFile(datasetsDir + "JetHT-Run2016C_b8aae01_XbC_p*.root");
        datasets.back().AddFile(datasetsDir + "JetHT-Run2016D_b8aae01_wNy_p*.root");
    }
    else
    {
        datasets.emplace_back(Dataset(Dataset::Process::QCD, Dataset::Generator::MadGraph,
          Dataset::ShowerGenerator::Pythia));
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-100-200-mg_3.1.1_Kah.root",
        //   27540000, 82095800);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-200-300-mg_3ac2b9b_ilj_p*.root",
          1717000, 38816448);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-300-500-mg_3ac2b9b_AXa_p*.root",
          351300, 16828258 + 37875602);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-300-500-ext1-mg_3ac2b9b_fMW_p*.root",
          351300, 16828258 + 37875602);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-500-700-mg_3ac2b9b_ENN_p*.root",
          31630, 18785349 + 44034159);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-500-700-ext1-mg_3ac2b9b_Nns_p*.root",
          31630, 18785349 + 44034159);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-700-1000-mg_3ac2b9b_ROK_p*.root",
          6802, 15621634 + 29832311);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-700-1000-ext1-mg_3ac2b9b_aTr_p*.root",
          6802, 15621634 + 29832311);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-1000-1500-mg_3ac2b9b_IAf.root",
          1206, 4889688 + 10327758);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-1000-1500-ext1-mg_3ac2b9b_CiT_p*.root",
          1206, 4889688 + 10327758);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-1500-2000-mg_3ac2b9b_vmO.root",
          120.4, 3928444);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-2000-inf-mg_3ac2b9b_hwT.root",
          25.25, 1992472);
    }
    
    
    // Add an additional location to seach for data files
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
    outputNameStream << "output/" << ((dataGroup == DatasetGroup::Data) ? "data" : "sim");
    
    if (systType != "None")
        outputNameStream << "_" << systType << "_" <<
          ((systDirection == SystService::VarDirection::Up) ? "up" : "down");
    
    outputNameStream << "/%";
    
    manager.RegisterService(new TFileService(outputNameStream.str()));
    
    manager.RegisterPlugin(new PECInputData);
    manager.RegisterPlugin(new PECPileUpReader);
    
    if (dataGroup == DatasetGroup::Data)
    {
        // Read original jets and MET, which have outdated corrections
        PECJetMETReader *jetmetReader = new PECJetMETReader("OrigJetMET");
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->ReadRawMET();
        manager.RegisterPlugin(jetmetReader);
        
        
        // Corrections to be applied to jets. They will also be propagated to MET.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        jetCorrFull->SetJEC({"Spring16_25nsV6_DATA_L1FastJet_AK4PFchs.txt",
          "Spring16_25nsV6_DATA_L2Relative_AK4PFchs.txt",
          "Spring16_25nsV6_DATA_L3Absolute_AK4PFchs.txt",
          "Spring16_25nsV6_DATA_L2Residual_AK4PFchs.txt"});
        manager.RegisterService(jetCorrFull);
        
        // L1 corrections to be used in T1 MET corrections
        JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
        jetCorrL1->SetJEC({"Spring16_25nsV6_DATA_L1RC_AK4PFchs.txt"});
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
        
        
        // Read original jets and MET
        PECJetMETReader *jetmetReader = new PECJetMETReader("OrigJetMET");
        jetmetReader->ReadRawMET();
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        manager.RegisterPlugin(jetmetReader);
        
        
        // Corrections to be applied to jets and also to be propagated to MET. Although original
        //jets in simulation already have up-to-date corrections, they will be reapplied in order
        //to have a consistent impact on MET from the stochastic JER smearing. The random-number
        //seed for the smearing is fixed for the sake of reproducibility.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        jetCorrFull->SetJEC({"Spring16_25nsV6_MC_L1FastJet_AK4PFchs.txt",
          "Spring16_25nsV6_MC_L2Relative_AK4PFchs.txt",
          "Spring16_25nsV6_MC_L3Absolute_AK4PFchs.txt"});
        jetCorrFull->SetJER("Spring16_25nsV6_MC_SF_AK4PFchs.txt",
          "Spring16_25nsV6_MC_PtResolution_AK4PFchs.txt", 4913);
        jetCorrFull->SetJECUncertainty("Spring16_25nsV6_MC_Uncertainty_AK4PFchs.txt");
        manager.RegisterService(jetCorrFull);
        
        // L1 corrections to be used in T1 MET corrections
        JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
        jetCorrL1->SetJEC({"Spring16_25nsV6_MC_L1RC_AK4PFchs.txt"});
        manager.RegisterService(jetCorrL1);
        
        
        // Recorrect jets and apply T1 MET corrections to raw MET
        JetMETUpdate *jetmetUpdater = new JetMETUpdate;
        jetmetUpdater->SetJetCorrection("JetCorrFull");
        jetmetUpdater->SetJetCorrectionForMET("JetCorrFull", "JetCorrL1", "", "");
        jetmetUpdater->UseRawMET();
        manager.RegisterPlugin(jetmetUpdater);
    }
    
    manager.RegisterPlugin(new TriggerBin({200., 250., 300., 370., 450., 510.}));
    manager.RegisterPlugin(new FirstJetFilter(0., 1.3));
    
    if (dataGroup == DatasetGroup::Data)
    {
        // Integrated luminosities are not used when collision data are processed. Only their
        //placeholders are given.
        manager.RegisterPlugin(new DynamicTriggerFilter({{"PFJet140", 1}, {"PFJet200", 1},
          {"PFJet260", 1}, {"PFJet320", 1}, {"PFJet400", 1}, {"PFJet450", 1}}));
    }
    else
    {
        // Apply trivial selection since trigger is not simulated
        manager.RegisterPlugin(new DynamicTriggerFilter({{"1", 12.387}, {"1", 59.689},
          {"1", 270.460}, {"1", 803.672}, {"1", 2307.991}, {"1", 12881.357}}));
    }
    
    if (dataGroup != DatasetGroup::Data)
    {
        string const version("ICHEP");
        manager.RegisterPlugin(new DynamicPileUpWeight(
          {"pileup_Run2016B_PFJet140_finebin_"s + version + ".root",
          "pileup_Run2016B_PFJet200_finebin_"s + version + ".root",
          "pileup_Run2016B_PFJet260_finebin_"s + version + ".root",
          "pileup_Run2016B_PFJet320_finebin_"s + version + ".root",
          "pileup_Run2016B_PFJet400_finebin_"s + version + ".root",
          "pileup_Run2016B_PFJet450_finebin_"s + version + ".root"},
          "simPUProfiles_80X.root", 0.05));
    }
    
    
    for (auto const &jetPtCut: jetPtCuts)
    {
        string const ptCutText(to_string(jetPtCut));
        
        RecoilBuilder *recoilBuilder = new RecoilBuilder("RecoilBuilderPt"s + ptCutText, jetPtCut);
        recoilBuilder->SetBalanceSelection(0.6, 0.3, 1.);
        recoilBuilder->SetBetaPtFraction(0.05);
        manager.RegisterPlugin(recoilBuilder, {"TriggerFilter"});
        //^ It is fine to specify the trigger filter as the dependency also in case of simulation
        //since DynamicPileUpWeight never rejects events
        
        BalanceVars *balanceVars = new BalanceVars("BalanceVarsPt"s + ptCutText);
        balanceVars->SetRecoilBuilderName(recoilBuilder->GetName());
        manager.RegisterPlugin(balanceVars);
    }
    
    // Pile-up information is saved only for the first jet pt threshold
    manager.RegisterPlugin(new PileUpVars, {"BalanceVarsPt"s + to_string(jetPtCuts.front())});
    
    
    // Process the datasets
    manager.Process(6);
    
    std::cout << '\n';
    manager.PrintSummary();
    
    
    return EXIT_SUCCESS;
}
