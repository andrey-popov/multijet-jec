/**
 * \file partition_runs.cpp
 *
 * A program to split a PEC file into multiple ones based on run numbers. Each partition includes
 * its left boundary. The in-file directory structure is reproduced.
 */

#include <EventID.hpp>

#include <TFile.h>
#include <TTree.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <regex>
#include <sstream>
#include <string>
#include <vector>


namespace fs = std::filesystem;
namespace po = boost::program_options;
using split_string = boost::algorithm::split_iterator<std::string::const_iterator>;


/**
 * \brief Recursively searches for trees in given file
 *
 * Returns a map from in-file paths to corresponding trees. The trees are owned by the given file
 * object.
 */
std::map<fs::path, TTree *> FindTrees(TFile &srcFile);

/**
 * \brief Splits given file into multiple based on run numbers
 *
 * The partition boundaries given as an argument must be sorted. A partition includes its left
 * boundary.
 */
void PartitionFile(fs::path const &inputPath, std::vector<int> const &runs);


int main(int argc, char **argv)
{
    po::options_description options("Supported options");
    options.add_options()
      ("input_files", po::value<std::vector<std::string>>(), "Input files")
      ("help,h", "Prints help message")
      ("runs,r", po::value<std::string>(), "Comma-separated list of runs for partitioning");

    po::positional_options_description positionalOptions;
    positionalOptions.add("input_files", -1);

    po::command_line_parser parser(argc, argv);
    parser.options(options);
    parser.positional(positionalOptions);
    
    po::variables_map optionMap;
    po::store(parser.run(), optionMap);

    if (optionMap.count("help"))
    {
        std::cerr << "Usage: partition_runs [options]\n";
        std::cerr << options << std::endl;
        return EXIT_FAILURE;
    }

    if (not optionMap.count("input_files"))
    {
        std::cerr << "No input files provided." << std::endl;
        return EXIT_FAILURE;
    }

    if (not optionMap.count("runs"))
    {
        std::cerr << "No run boundaries provided." << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<int> runs;
    std::string const runsText = optionMap["runs"].as<std::string>();
    split_string it = boost::algorithm::make_split_iterator(
      runsText, boost::algorithm::token_finder([](char const c){return c == ',';}));

    for (; it != split_string(); ++it)
        runs.emplace_back(boost::lexical_cast<int>(*it));

    if (not std::is_sorted(runs.begin(), runs.end()))
    {
        std::cerr << "Provided list of runs is not sorted." << std::endl;
        return EXIT_FAILURE;
    }


    for (fs::path const path: optionMap["input_files"].as<std::vector<std::string>>())
    {
        std::cerr << "Processing file " << path << std::endl;
        PartitionFile(path, runs);
    }


    return EXIT_SUCCESS;
}


void PartitionFile(fs::path const &inputPath, std::vector<int> const &runs)
{
    std::string const filename{inputPath.filename()};
    std::regex const filenameRegex{"^(.*?)((\\.part\\d+)\\.root)$"};
    std::smatch matchResults;

    if (not std::regex_match(filename, matchResults, filenameRegex))
    {
        std::cerr << "Unexpected format of filename in " << inputPath << "." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    TFile inputFile{inputPath.c_str()};
    
    if (inputFile.IsZombie())
    {
        std::cerr << "Failed to open file " << inputPath << "." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::vector<std::unique_ptr<TFile>> outputFiles;

    for (unsigned i = 0; i < runs.size() + 1; ++i)
    {
        std::ostringstream name;
        name << matchResults[1] << i + 1 << matchResults[2];
        outputFiles.emplace_back(new TFile(name.str().c_str(), "recreate"));
    }


    // Find trees in the input file and create their empty clones in all output files
    auto srcTrees = FindTrees(inputFile);
    std::vector<std::map<fs::path, TTree *>> outTrees{outputFiles.size()};

    for (unsigned iFile = 0; iFile < outputFiles.size(); ++iFile)
    {
        auto &outputFile = outputFiles[iFile];

        for (auto &[path, srcTree]: srcTrees)
        {
            auto const curDirectoryPath = path.parent_path();
            outputFile->mkdir(curDirectoryPath.c_str());
            outputFile->cd(curDirectoryPath.c_str());

            outTrees[iFile][path] = srcTree->CloneTree(0);
            // The clonned tree is associated with the current directory in the output file
        }
    }


    // Set up reading of event ID trees, which is needed for the partitioning
    auto srcEventIdTree = srcTrees.at("pecEventID/EventID");
    pec::EventID *eventId = nullptr;
    srcEventIdTree->SetBranchAddress("eventId", &eventId);


    // Copy entries to output trees, while keeping track of the number of entries written
    std::vector<int64_t> outCounts(outputFiles.size(), 0);

    for (int64_t iEntry = 0; iEntry < srcEventIdTree->GetEntries(); ++iEntry)
    {
        for (auto p: srcTrees)
            p.second->GetEntry(iEntry);

        int const curRun = eventId->RunNumber();
        int const partition = std::upper_bound(runs.begin(), runs.end(), curRun) - runs.begin();

        for (auto p: outTrees[partition])
            p.second->Fill();

        ++outCounts[partition];
    }


    for (auto &outputFile: outputFiles)
    {
        outputFile->Write();
        outputFile->Close();
    }

    inputFile.Close();


    // If there happen to be empty output files, remove them
    for (unsigned i = 0; i < outCounts.size(); ++i)
    {
        if (outCounts[i] == 0)
        {
            fs::path const path{outputFiles[i]->GetName()};
            std::cerr << "Output file " << path << " is empty. Removing it." << std::endl;
            fs::remove(path);
        }
    }
}


std::map<fs::path, TTree *> FindTrees(TFile &srcFile)
{
    std::map<fs::path, TTree *> treeMap;
    std::queue<fs::path> directories;
    directories.push("");

    while (not directories.empty())
    {
        auto const curDirectoryPath = directories.front();
        directories.pop();

        TDirectoryFile *curDirectory;

        if (curDirectoryPath.empty())
            curDirectory = &srcFile;
        else
            curDirectory = dynamic_cast<TDirectoryFile *>(srcFile.Get(curDirectoryPath.c_str()));

        for (TObject *keyObj: *curDirectory->GetListOfKeys())
        {
            TKey * const key = dynamic_cast<TKey *>(keyObj);
            std::string const name{key->GetName()};
            std::string const className{key->GetClassName()};

            if (className == "TDirectoryFile")
                directories.push(curDirectoryPath / name);
            else if (className == "TTree")
                treeMap[curDirectoryPath / name] = dynamic_cast<TTree *>(key->ReadObj());
            else
            {
                std::cerr << "Object " << curDirectoryPath / name << " has unexpected type \"" <<
                  className << "\"." << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }
    }

    if (treeMap.empty())
    {
        std::cerr << "No trees found in input file." << std::endl;
        std::exit(EXIT_FAILURE);
    }


    // As a sanity check, make sure that all trees have the same number of entries
    auto treeIt = treeMap.begin();
    int64_t const numEntries = treeIt->second->GetEntries();
    ++treeIt;

    while (treeIt != treeMap.end())
    {
        if (treeIt->second->GetEntries() != numEntries)
            std::cerr << "WARNING: Numbers of entries in source trees do not agree." << std::endl;

        ++treeIt;
    }

    return treeMap;
}

