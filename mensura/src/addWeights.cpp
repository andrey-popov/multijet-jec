/**
 * This program reads simulation a simulation file created by program multijet and computes total
 * event weights that account for the integrated luminosity and pileup reweighting.
 * 
 * Luminosities and paths to files with target pileup profiles for reweighting are read from a JSON
 * file with the following structure:
 *   [
 *     {
 *       "index": 1,
 *       "trigger": "PFJet140",
 *       "lumi": 12.387,
 *       "pileupProfile": "pileup_Run2016B_PFJet140_finebin_ICHEP.root"
 *     },
 *     // ...
 *   ]
 * 
 * Inputs for the computation of weights are read from trees "BalanceVars{label}" and
 * "PileUpVars{label}", where {label} is an arbitrary label. If multiple sets of trees with
 * different labels are found, weights are computed for each one. The results are stored in trees
 * "Weights{label}" in a new file, which is created in the current directory. The name of the
 * output file is formed by adding to the name of the input file a string "_weights{postfix}" with
 * an optional {postfix} given by the user.
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
 * \brief Parses JSON file with details about trigger bins
 * 
 * Extracts luminosities and names of files with target pileup profiles and returns them in a map.
 * Map key is the trigger bin index.
 */
std::map<unsigned, std::tuple<double, std::string>> ParseInfoFile(std::string const &infoFileName)
{
    using namespace std;
    
    
    // Open the JSON file and perform basic validation
    ifstream infoFile(infoFileName, std::ifstream::binary);
    Json::Value parsedInfoFile;
    
    if (not infoFile.is_open())
    {
        BadInputError excp;
        excp << "File \"" << infoFileName << "\" does not exist or cannot be opened.";
        throw excp;
    }
    
    try
    {
        infoFile >> parsedInfoFile;
    }
    catch (Json::Exception const &)
    {
        BadInputError excp;
        excp << "Failed to parse file \"" << infoFileName << "\" as JSON.";
        throw excp;
    }
    
    infoFile.close();
    
    if (not parsedInfoFile.isArray())
    {
        BadInputError excp;
        excp << "File \"" << infoFileName << "\" does not contain an array at the top level.";
        throw excp;
    }
    
    if (not parsedInfoFile.size())
    {
        BadInputError excp;
        excp << "Array of trigger bins in file \"" << infoFileName << "\" is empty.";
        throw excp;
    }
    
    
    // Extract details about each trigger bin
    map<unsigned, tuple<double, string>> triggerBins;
    
    for (unsigned iBin = 0; iBin < parsedInfoFile.size(); ++iBin)
    {
        auto const &binInfo = parsedInfoFile[iBin];
        
        if (not binInfo.isMember("index") or not binInfo["index"].isInt())
        {
            BadInputError excp;
            excp << "Entry #" << iBin << " in file \"" << infoFileName <<
              "\" does not contain required field \"index\", or its value is not an integer "
              "number.";
            throw excp;
        }
        
        if (not binInfo.isMember("lumi") or not binInfo["lumi"].isNumeric())
        {
            BadInputError excp;
            excp << "Entry #" << iBin << " in file \"" << infoFileName <<
              "\" does not contain required field \"lumi\", or its value is not a number.";
            throw excp;
        }
        
        if (not binInfo.isMember("pileupProfile") or not binInfo["pileupProfile"].isString())
        {
            BadInputError excp;
            excp << "Entry #" << iBin << " in file \"" << infoFileName <<
              "\" does not contain required field \"pileupProfile\", or its value is not a "
              "string.";
            throw excp;
        }
        
        triggerBins.emplace(piecewise_construct, forward_as_tuple(binInfo["index"].asInt()),
          forward_as_tuple(binInfo["lumi"].asDouble(), binInfo["pileupProfile"].asString()));
    }
    
    
    return triggerBins;
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
    
    
    // Open input file and check if it contains required trees. Their names are predefined except
    //for potential arbitrary postfix.
    TFile inputFile(inputFileName.c_str());
    
    if (inputFile.IsZombie())
    {
        cerr << "File \"" << inputFileName << "\" does not exist, or it is not a valid ROOT "
          "file.\n";
        return EXIT_FAILURE;
    }
    
    string const mainTreeName("BalanceVars");
    string const pileupTreeName("PileUpVars");
    
    list<string> versions;
    TIter fileIter(inputFile.GetListOfKeys());
    TKey *key;
    
    while ((key = dynamic_cast<TKey *>(fileIter())))
    {
        if (strcmp(key->GetClassName(), "TTree") != 0)
            continue;
        
        if (boost::starts_with(key->GetName(), mainTreeName))
        {
            string const postfix(string(key->GetName()).substr(11));
            versions.emplace_back(postfix);
            
            if (not inputFile.Get((pileupTreeName + postfix).c_str()))
            {
                cerr << "File \"" << inputFileName << "\" contains a tree \"" << key->GetName() <<
                  "\", but corresponding tree with pileup information with name \"" <<
                  pileupTreeName + postfix << "\" is not found.\n";
                return EXIT_FAILURE;
            }
        }
    }
    
    if (versions.empty())
    {
        cerr << "Failed to find required trees in file \"" << inputFileName << "\".\n";
        return EXIT_FAILURE;
    }
    
    
    // Parse the JSON file with luminosities and pileup profiles
    map<unsigned, tuple<double, string>> triggerBinsInfo;
    
    try
    {
        triggerBinsInfo = ParseInfoFile(infoFileName);
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
    
    
    // Read target pileup profiles and construct reweighting objects. The objects are put into a
    //map whose key is the index of the trigger bin.
    map<unsigned, ReweighterObject> reweighters;
    
    for (auto const &binInfo: triggerBinsInfo)
    {
        // Read target pileup profile
        string const fileName(FileInPath::Resolve("PileUp/", get<1>(binInfo.second)));
        TFile file(fileName.c_str());
        unique_ptr<TH1> targetPUProfile(dynamic_cast<TH1 *>(file.Get("pileup")));
        
        if (not targetPUProfile)
        {
            cerr << "Failed to read target pileup profile from file \"" << fileName << "\".\n";
            return EXIT_FAILURE;
        }
        
        targetPUProfile->SetDirectory(nullptr);
        file.Close();
        
        
        // Create the reweighting object
        reweighters.emplace(piecewise_construct, forward_as_tuple(binInfo.first),
          forward_as_tuple(get<0>(binInfo.second), move(targetPUProfile), simPUProfile));
    }
    
    
    // Create the output file. It is created in the current directory, and its name is based on the
    //name of the input file.
    auto const posNameBegin = inputFileName.find_last_of('/') + 1;
    auto const posNameEnd = inputFileName.find_last_of('.');
    string const outFileName = inputFileName.substr(posNameBegin, posNameEnd - posNameBegin) +
      "_weights" + outFilePostfix + ".root";
    
    TFile outFile(outFileName.c_str(), "recreate");
    
    
    // Create output trees with weights one by one
    for (auto const &version: versions)
    {
        // Set up branches and create output tree
        TTree *inputTree = dynamic_cast<TTree *>(inputFile.Get((mainTreeName + version).c_str()));
        inputTree->AddFriend((pileupTreeName + version).c_str());
        
        inputTree->SetBranchStatus("*", false);
        
        for (auto const &branchName: {"TriggerBin", "WeightDataset", "LambdaPU"})
            inputTree->SetBranchStatus(branchName, true);
        
        UShort_t triggerBin;
        Float_t weightDataset, lambdaPU;
        
        inputTree->SetBranchAddress("TriggerBin", &triggerBin);
        inputTree->SetBranchAddress("WeightDataset", &weightDataset);
        inputTree->SetBranchAddress("LambdaPU", &lambdaPU);
        
        outFile.cd();
        TTree outTree(("Weights"s + version).c_str(), "Event weights");
        
        Float_t totalWeight[3];
        outTree.Branch("TotalWeight", totalWeight, "TotalWeight[3]/F");
        
        
        // Fill the output tree
        for (long ev = 0; ev < inputTree->GetEntries(); ++ev)
        {
            inputTree->GetEntry(ev);
            auto const &weights = reweighters.at(triggerBin).GetWeights(lambdaPU);
            
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
