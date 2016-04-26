#include <BalanceVars.hpp>
#include <DynamicPileUpWeight.hpp>
#include <DynamicTriggerFilter.hpp>
#include <RecoilBuilder.hpp>

#include <mensura/core/Dataset.hpp>
#include <mensura/core/FileInPath.hpp>
#include <mensura/core/RunManager.hpp>

#include <mensura/extensions/JetFilter.hpp>
#include <mensura/extensions/PileUpWeight.hpp>
#include <mensura/extensions/TFileService.hpp>

#include <mensura/PECReader/PECInputData.hpp>
#include <mensura/PECReader/PECJetMETReader.hpp>
#include <mensura/PECReader/PECPileUpReader.hpp>

#include <cstdlib>
#include <iostream>
#include <list>


using namespace std;


enum class DatasetGroup
{
    Data,
    MC
};


int main(int argc, char **argv)
{
    // Parse arguments
    if (argc != 2)
    {
        cerr << "Usage: multijet dataset-group\n";
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
    
    
    
    // Input datasets
    list<Dataset> datasets;
    string const datasetsDir("/gridgroup/cms/popov/Analyses/JetMET/2016.04.11_Grid-campaign/"
      "Results/");
    
    if (dataGroup == DatasetGroup::Data)
    {
        datasets.emplace_back(Dataset({Dataset::Process::ppData, Dataset::Process::pp13TeV},
          Dataset::Generator::Nature, Dataset::ShowerGenerator::Nature));
        // datasets.back().AddFile(datasetsDir + "JetHT-Run2015*.root");
        datasets.back().AddFile(datasetsDir + "JetHT-Run2015D_3.1.1_sqh_p1.root");
    }
    else
    {
        datasets.emplace_back(Dataset(Dataset::Process::QCD, Dataset::Generator::MadGraph,
          Dataset::ShowerGenerator::Pythia));
        datasets.back().AddFile(datasetsDir + "QCD-Ht-100-200-mg_3.1.1_Kah.root",
          27540000 * 0.131, 82095800);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-200-300-mg_3.1.1_ilS.root",
          1717000 * 0.098, 18784379);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-300-500-mg_3.1.1_UpJ_p*.root",
        //   351300 * 0.088, 16909004);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-500-700-mg_3.1.1_XWW_p*.root",
        //   31630 * 0.067, 19665695);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-700-1000-mg_3.1.1_mtk_p*.root",
        //   6802 * 0.066, 13801981);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-1000-1500-mg_3.1.1_MoZ.root",
        //   1206 * 0.059, 5049267 + 10144378);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-1000-1500-mg_ext1_3.1.1_pTT_p*.root",
        //   1206 * 0.059, 5049267 + 10144378);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-1500-2000-mg_3.1.1_mIr.root",
        //   120.4 * 0.067, 3939077);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-2000-inf-mg_3.1.1_DTg.root",
        //   25.25 * 0.07, 1981228 + 4016332);
        // datasets.back().AddFile(datasetsDir + "QCD-Ht-2000-inf-mg_ext1_3.1.1_Nlw.root",
        //   25.25 * 0.07, 1981228 + 4016332);
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
    
    
    // Register services
    manager.RegisterService(new TFileService("output/%"));
    
    
    // Register plugins
    manager.RegisterPlugin(new PECInputData);
    
    PECJetMETReader *jetReader = new PECJetMETReader;
    jetReader->ConfigureLeptonCleaning("");  // Disabled
    manager.RegisterPlugin(jetReader);
    
    RecoilBuilder *recoilBuilder = new RecoilBuilder(30., {210., 290., 370., 470., 550., 610.});
    recoilBuilder->SetBalanceSelection(0.6, 0.3, 1.);
    manager.RegisterPlugin(recoilBuilder);
    
    manager.RegisterPlugin(new DynamicTriggerFilter({{"PFJet140", 2.896}, {"PFJet200", 19.898},
      {"PFJet260", 202.492}, {"PFJet320", 423.595}, {"PFJet400", 938.067},
      {"PFJet450", 2312.360}}));
    
    if (dataGroup != DatasetGroup::Data)
    {
        manager.RegisterPlugin(new PECPileUpReader);
        manager.RegisterPlugin(new DynamicPileUpWeight({"pileup_Run2015CD_PFJet140_finebin.root",
          "pileup_Run2015CD_PFJet200_finebin.root", "pileup_Run2015CD_PFJet260_finebin.root",
          "pileup_Run2015CD_PFJet320_finebin.root", "pileup_Run2015CD_PFJet400_finebin.root",
          "pileup_Run2015CD_PFJet450_finebin.root"}, "simPUProfiles_76X.root", 0.05));
    }
    
    manager.RegisterPlugin(new BalanceVars);
    
    
    // Process the datasets
    manager.Process(1);
    
    
    return EXIT_SUCCESS;
}
