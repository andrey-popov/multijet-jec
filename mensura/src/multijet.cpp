#include <BalanceVars.hpp>
#include <DynamicPileUpWeight.hpp>
#include <DynamicTriggerFilter.hpp>
#include <RecoilBuilder.hpp>
#include <TriggerBin.hpp>

#include <mensura/core/Dataset.hpp>
#include <mensura/core/FileInPath.hpp>
#include <mensura/core/RunManager.hpp>

#include <mensura/extensions/JetCorrectorService.hpp>
#include <mensura/extensions/JetFilter.hpp>
#include <mensura/extensions/JetMETUpdate.hpp>
#include <mensura/extensions/PileUpWeight.hpp>
#include <mensura/extensions/TFileService.hpp>

#include <mensura/PECReader/PECInputData.hpp>
#include <mensura/PECReader/PECJetMETReader.hpp>
#include <mensura/PECReader/PECPileUpReader.hpp>

#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>


using namespace std;


enum class DatasetGroup
{
    Data,
    MC
};


int main(int argc, char **argv)
{
    // Parse arguments
    if (argc != 2 and argc != 3)
    {
        cerr << "Usage: multijet dataset-group [jet-pt-cuts]\n";
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
    
    
    // Input datasets
    list<Dataset> datasets;
    string const datasetsDir("/gridgroup/cms/popov/Analyses/JetMET/2016.05.21_Grid-campaign-80X/"
      "Results/");
    
    if (dataGroup == DatasetGroup::Data)
    {
        datasets.emplace_back(Dataset({Dataset::Process::ppData, Dataset::Process::pp13TeV},
          Dataset::Generator::Nature, Dataset::ShowerGenerator::Nature));
        datasets.back().AddFile(datasetsDir + "JetHT-Run2016*.root");
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
    manager.RegisterService(new TFileService("output/%"));
    manager.RegisterPlugin(new PECInputData);
    manager.RegisterPlugin(new PECPileUpReader);
    
    if (dataGroup == DatasetGroup::Data)
    {
        // Read jets with outdated corrections
        PECJetMETReader *jetReader = new PECJetMETReader("OrigJetMET");
        jetReader->ConfigureLeptonCleaning("");  // Disabled
        manager.RegisterPlugin(jetReader);
        
        
        // Corrections to be applied to jets. Same ones will be propagated to MET.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        jetCorrFull->SetJEC({"Spring16_25nsV2_DATA_L1FastJet_AK4PFchs.txt",
          "Spring16_25nsV2_DATA_L2Relative_AK4PFchs.txt",
          "Spring16_25nsV2_DATA_L3Absolute_AK4PFchs.txt",
          "Spring16_25ns_MPF_LOGLIN_L2Residual_pythia8_v3_AK4PFchs.txt"});
        manager.RegisterService(jetCorrFull);
        
        // Corrections applied in computation of T1-corrected MET. Needed to undo this MET
        //correction.
        JetCorrectorService *jetCorrOrig = new JetCorrectorService("JetCorrOrig");
        jetCorrOrig->SetJEC({"Spring16_25nsV2_DATA_L1FastJet_AK4PFchs.txt",
          "Spring16_25nsV2_DATA_L2Relative_AK4PFchs.txt",
          "Spring16_25nsV2_DATA_L3Absolute_AK4PFchs.txt",
          "Spring16_25nsV2_DATA_L2L3Residual_AK4PFchs.txt"});
        manager.RegisterService(jetCorrOrig);
        
        
        // Plugin to update jets and MET. L1 JEC for MET are not specified because they do not
        //differ between the original and new JEC.
        JetMETUpdate *jetmetUpdater = new JetMETUpdate;
        jetmetUpdater->SetJetCorrection("JetCorrFull");
        jetmetUpdater->SetJetCorrectionForMET("JetCorrFull", "", "JetCorrOrig", "");
        manager.RegisterPlugin(jetmetUpdater);
    }
    else
    {
        // In simulation jets have proper corrections out of the box
        PECJetMETReader *jetReader = new PECJetMETReader;
        jetReader->ConfigureLeptonCleaning("");  // Disabled
        manager.RegisterPlugin(jetReader);
    }
    
    manager.RegisterPlugin(new TriggerBin({210., 290., 370., 470., 550., 610.}));
    
    if (dataGroup == DatasetGroup::Data)
        manager.RegisterPlugin(new DynamicTriggerFilter({{"PFJet140", 2.667},
          {"PFJet200", 6.823}, {"PFJet260", 35.721}, {"PFJet320", 118.180},
          {"PFJet400", 258.870}, {"PFJet450", 581.021}}));
    else
    {
        // Apply trivial selection since trigger is not simulated
        manager.RegisterPlugin(new DynamicTriggerFilter({{"1", 2.667}, {"1", 6.823},
          {"1", 35.721}, {"1", 118.180}, {"1", 258.870}, {"1", 581.021}}));
    }
    
    if (dataGroup != DatasetGroup::Data)
    {
        manager.RegisterPlugin(new DynamicPileUpWeight({"pileup_Run2016B_PFJet140_finebin_v2.root",
          "pileup_Run2016B_PFJet200_finebin_v2.root", "pileup_Run2016B_PFJet260_finebin_v2.root",
          "pileup_Run2016B_PFJet320_finebin_v2.root", "pileup_Run2016B_PFJet400_finebin_v2.root",
          "pileup_Run2016B_PFJet450_finebin_v2.root"}, "simPUProfiles_80X.root", 0.05));
    }
    
    
    for (auto const &jetPtCut: jetPtCuts)
    {
        string const ptCutText(to_string(jetPtCut));
        
        RecoilBuilder *recoilBuilder = new RecoilBuilder("RecoilBuilderPt"s + ptCutText, jetPtCut);
        recoilBuilder->SetBalanceSelection(0.6, 0.3, 1.);
        recoilBuilder->SetBetaPtFraction(0.05);
        manager.RegisterPlugin(recoilBuilder);
        
        BalanceVars *balanceVars = new BalanceVars("BalanceVarsPt"s + ptCutText);
        balanceVars->SetRecoilBuilderName(recoilBuilder->GetName());
        manager.RegisterPlugin(balanceVars);
    }
    
    
    // Process the datasets
    manager.Process(6);
    
    
    return EXIT_SUCCESS;
}
