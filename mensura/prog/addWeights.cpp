/**
 * This program reads a simulation file created by program multijet and computes total event
 * weights that account for the integrated luminosity and observed pileup profile.
 * 
 * Luminosities and paths to files with target pileup profiles for reweighting are read from a JSON
 * file with the following structure:
 *   {
 *     "PFJet140": {
 *       "lumi": 12.387,
 *       "pileupProfile": "pileup_Run2016B_PFJet140_finebin_ICHEP.root"
 *     },
 *     // ...
 *   }
 * 
 * Inputs for the computation of weights are read from trees "{trigger}/BalanceVars" and
 * "{trigger}/PileUpVars", where {trigger} is a trigger name. The trigger name is also used as the
 * key in the JSON configuration file. The computation is performed for all triggers included in
 * the simulation file. The results are stored in trees "{trigger}/Weights" in a new file, which is
 * created in the current directory. The name of the output file is formed by adding to the name of
 * the input file a string "_weights{postfix}" with an optional {postfix} given by the user.
 */

#include <mensura/core/FileInPath.hpp>
#include <mensura/external/JsonCpp/json.hpp>

#include <TFile.h>
#include <TH1.h>
#include <TKey.h>
#include <TTree.h>

#include <boost/algorithm/string/predicate.hpp>

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>


/*************************************************************************************************/


/**
 * \class ReweighterObject
 * \brief A class to compute weights for luminosity and pileup
 */
class ReweighterObject
{
public:
    /// Constructor from integrated luminosity (in 1/pb) and pileup profiles
    ReweighterObject(double lumi, std::unique_ptr<TH1> &&targerPUProfile,
      std::shared_ptr<TH1> simPUProfile, double puSystError = 0.05);
    
public:
    /**
     * \brief Computes event weights
     * 
     * Returns an array with nominal weight and varied weights to reflect systematic uncertainty
     * in pileup reweighting.
     */
    std::array<double, 3> const &GetWeights(double lambdaPU);
    
private:
    /// Integrated luminosity, 1/pb
    double lumi;
    
    /// Targer pileup profile
    std::unique_ptr<TH1> targerPUProfile;
    
    /// Simulated pileup profile
    std::shared_ptr<TH1> simPUProfile;
    
    /// Systematical uncertainty for pileup
    double puSystError;
    
    /// Buffer to store computed weights
    std::array<double, 3> weights;
};


ReweighterObject::ReweighterObject(double lumi_, std::unique_ptr<TH1> &&targerPUProfile_,
  std::shared_ptr<TH1> simPUProfile_, double puSystError_ /*= 0.05*/):
    lumi(lumi_),
    targerPUProfile(std::move(targerPUProfile_)),
    simPUProfile(simPUProfile_),
    puSystError(puSystError_)
{
    // Make sure the target profile is normalized
    targerPUProfile->Scale(1. / targerPUProfile->Integral(0, -1), "width");
}


std::array<double, 3> const &ReweighterObject::GetWeights(double lambdaPU)
{
    // Compute pileup probability according to the simulated profile
    unsigned bin = simPUProfile->FindFixBin(lambdaPU);
    double const simProb = simPUProfile->GetBinContent(bin);
    
    if (simProb <= 0.)
    {
        weights = {0., 0., 0.};
        return weights;
    }
    
    
    // Compute pileup weights
    bin = targerPUProfile->FindFixBin(lambdaPU);
    weights.at(0) = targerPUProfile->GetBinContent(bin) / simProb;
    
    bin = targerPUProfile->FindFixBin(lambdaPU * (1. + puSystError));
    weights.at(1) = targerPUProfile->GetBinContent(bin) / simProb * (1. + puSystError);
    //^ The last multiplier corrects for the change in the total normalization due to the rescale
    //of the integration variable. Same applies to the "down" weight below.
    
    bin = targerPUProfile->FindFixBin(lambdaPU * (1. - puSystError));
    weights.at(2) = targerPUProfile->GetBinContent(bin) / simProb * (1. - puSystError);
    
    
    // Include luminosity into the weights
    for (auto &w: weights)
        w *= lumi;
    
    
    return weights;
}


/*************************************************************************************************/


/**
 * \brief BadInputError
 * \brief Exception class to describe errors due to ill-formed or unexpected inputs
 */
class BadInputError: public std::exception
{
public:
    /// Constructor from an error message
    BadInputError(std::string initialMessage = "");
    
    /// Move constructor
    BadInputError(BadInputError &&other);
    
public:
    /// Inserts formatted data into the message
    template<typename T>
    std::ostream &operator<<(T const &value);
    
    /// Returns error message
    virtual const char *what() const noexcept override;
    
private:
    /// Error message
    std::ostringstream message;
};


BadInputError::BadInputError(std::string initialMessage /*= ""*/):
    std::exception()
{
    message << initialMessage;
}


BadInputError::BadInputError(BadInputError &&other):
    std::exception(other),
    message(other.message.str())
{}


template<typename T>
std::ostream & BadInputError::operator<<(T const &value)
{
    return message << value;
}


const char *BadInputError::what() const noexcept
{
    return message.str().c_str();
}


/*************************************************************************************************/


/**
 * \brief Parses JSON file with details about triggers
 * 
 * Extracts luminosities and names of files with target pileup profiles and returns them in a map.
 * Map key is the trigger name.
 */
std::map<std::string, std::tuple<double, std::string>> ParseInfoFile(
  std::string const &infoFileName)
{
    using namespace std;
    
    
    // Open the JSON file and perform basic validation
    ifstream infoFile(infoFileName, std::ifstream::binary);
    Json::Value root;
    
    if (not infoFile.is_open())
    {
        BadInputError excp;
        excp << "File \"" << infoFileName << "\" does not exist or cannot be opened.";
        throw excp;
    }
    
    try
    {
        infoFile >> root;
    }
    catch (Json::Exception const &)
    {
        BadInputError excp;
        excp << "Failed to parse file \"" << infoFileName << "\" as JSON.";
        throw excp;
    }
    
    infoFile.close();
    
    if (not root.isObject())
    {
        BadInputError excp;
        excp << "File \"" << infoFileName << "\" does not contain a dictionary at the top level.";
        throw excp;
    }
    
    
    // Extract details about each trigger bin
    map<string, tuple<double, string>> triggerInfos;
    
    for (auto const &triggerName: root.getMemberNames())
    {
        auto const &entry = root[triggerName];
        
        if (not entry.isObject())
        {
            BadInputError excp;
            excp << "Entry \"" << triggerName << "\" in file \"" << infoFileName <<
              "\" is not a dictionary.";
            throw excp;
        }
        
        if (not entry.isMember("lumi") or not entry.isMember("pileupProfile"))
        {
            BadInputError excp;
            excp << "Entry \"" << triggerName << "\" in file \"" << infoFileName <<
              "\" does not contain required field \"lumi\" or \"pileupProfile\".";
            throw excp;
        }
        
        triggerInfos.emplace(piecewise_construct, forward_as_tuple(triggerName),
          forward_as_tuple(entry["lumi"].asDouble(), entry["pileupProfile"].asString()));
    }
    
    
    return triggerInfos;
}


/*************************************************************************************************/


int main(int argc, char **argv)
{
    using namespace std;
    
    
    // Parse arguments
    if (argc < 3 or argc > 4 or (argc > 1 and strcmp(argv[1], "-h") == 0))
    {
        cerr << "Usage: addWeights inputFile.root triggerBins.json [outFilePostfix]\n";
        return EXIT_FAILURE;
    }
    
    string const inputFileName(argv[1]);
    string const infoFileName(argv[2]);
    string const outFilePostfix((argc > 3) ? argv[3] : "");
    
    
    // Open input file and make sure that all trees are present
    TFile inputFile(inputFileName.c_str());
    
    if (inputFile.IsZombie())
    {
        cerr << "File \"" << inputFileName << "\" does not exist, or it is not a valid ROOT "
          "file.\n";
        return EXIT_FAILURE;
    }
    
    string const mainTreeName("BalanceVars");
    string const pileupTreeName("PileUpVars");
    
    list<string> triggers;
    TIter fileIter(inputFile.GetListOfKeys());
    TKey *key;
    
    while ((key = dynamic_cast<TKey *>(fileIter())))
    {
        if (strcmp(key->GetClassName(), "TDirectoryFile") != 0)
            continue;
        
        string const triggerName(key->GetName());
        triggers.emplace_back(triggerName);
        
        for (auto const &treeName: {mainTreeName, pileupTreeName})
            if (not inputFile.Get((triggerName + "/" + treeName).c_str()))
            {
                cerr << "File \"" << inputFileName << "\" does not contain required tree \"" <<
                  triggerName << "/" << treeName << "\".\n";
                return EXIT_FAILURE;
            }
    }
    
    if (triggers.empty())
    {
        cerr << "Failed to find required trees in file \"" << inputFileName << "\".\n";
        return EXIT_FAILURE;
    }
    
    
    // Parse the JSON file with luminosities and pileup profiles
    map<string, tuple<double, string>> triggerInfos;
    
    try
    {
        triggerInfos = ParseInfoFile(infoFileName);
    }
    catch (BadInputError const &excp)
    {
        cerr << excp.what() << '\n';
        return EXIT_FAILURE;
    }
    
    
    // Add an additional location to seach for files with pileup profiles
    char const *installPath = getenv("MULTIJET_JEC_INSTALL");
    
    if (not installPath)
    {
        cerr << "Mandatory environmental variable MULTIJET_JEC_INSTALL is not defined.\n";
        return EXIT_FAILURE;
    }
    
    FileInPath::AddLocation(string(installPath) + "/data/");
    
    
    // Read simulated pileup profile
    string const simPUProfileFileName(
      FileInPath::Resolve("PileUp/", "simPUProfiles_80Xv2.root"));
    TFile simPUProfileFile(simPUProfileFileName.c_str());
    shared_ptr<TH1> simPUProfile(dynamic_cast<TH1 *>(simPUProfileFile.Get("nominal")));
    
    if (not simPUProfile)
    {
        cerr << "Failed to read simulated pileup profile.\n";
        return EXIT_FAILURE;
    }
    
    simPUProfile->SetDirectory(nullptr);
    simPUProfileFile.Close();
    
    // Make sure the profile is normalized
    simPUProfile->Scale(1. / simPUProfile->Integral(0, -1), "width");
    
    
    // Create the output file. It is created in the current directory, and its name is based on the
    //name of the input file.
    auto const posNameBegin = inputFileName.find_last_of('/') + 1;
    auto const posNameEnd = inputFileName.find_last_of('.');
    string const outFileName = inputFileName.substr(posNameBegin, posNameEnd - posNameBegin) +
      "_weights" + outFilePostfix + ".root";
    
    TFile outFile(outFileName.c_str(), "recreate");
    
    
    // Create output trees with weights one by one
    for (auto const &trigger: triggers)
    {
        auto const triggerInfoRes = triggerInfos.find(trigger);
        
        if (triggerInfoRes == triggerInfos.end())
        {
            cerr << "No information is available for trigger \"" << trigger <<
              "\" in the configuration file.\n";
            return EXIT_FAILURE;
        }
        
        auto const &triggerInfo = triggerInfoRes->second;
        
        
        // Read target pileup profile and construct the reweighting object
        string const puProfileFileName(FileInPath::Resolve("PileUp/", get<1>(triggerInfo)));
        TFile puProfileFile(puProfileFileName.c_str());
        unique_ptr<TH1> targetPUProfile(dynamic_cast<TH1 *>(puProfileFile.Get("pileup")));
        
        if (not targetPUProfile)
        {
            cerr << "Failed to read target pileup profile from file \"" << puProfileFileName <<
              "\".\n";
            return EXIT_FAILURE;
        }
        
        targetPUProfile->SetDirectory(nullptr);
        puProfileFile.Close();
        
        ReweighterObject reweighter(get<0>(triggerInfo), move(targetPUProfile), simPUProfile);
        
        
        // Set up branches and create output tree
        TTree *inputTree = dynamic_cast<TTree *>(
          inputFile.Get((trigger + "/" + mainTreeName).c_str()));
        inputTree->AddFriend((trigger + "/" + pileupTreeName).c_str());
        
        inputTree->SetBranchStatus("*", false);
        
        for (auto const &branchName: {"WeightDataset", "LambdaPU"})
            inputTree->SetBranchStatus(branchName, true);
        
        Float_t weightDataset, lambdaPU;
        
        inputTree->SetBranchAddress("WeightDataset", &weightDataset);
        inputTree->SetBranchAddress("LambdaPU", &lambdaPU);
        
        outFile.cd();
        TDirectory *outDirectory = outFile.GetDirectory(trigger.c_str());
        
        if (not outDirectory)
            outDirectory = outFile.mkdir(trigger.c_str());
        
        outDirectory->cd();
        TTree outTree("Weights", "Event weights");
        
        Float_t totalWeight[3];
        outTree.Branch("TotalWeight", totalWeight, "TotalWeight[3]/F");
        
        
        // Fill the output tree
        for (long ev = 0; ev < inputTree->GetEntries(); ++ev)
        {
            inputTree->GetEntry(ev);
            auto const &weights = reweighter.GetWeights(lambdaPU);
            
            for (unsigned i = 0; i < 3; ++i)
                totalWeight[i] = weights[i] * weightDataset;
            
            outTree.Fill();
        }
        
        
        // Write the tree
        outTree.Write();
    }
    
    
    outFile.Close();
    inputFile.Close();
    
    
    return EXIT_SUCCESS;
}
