#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <L1TPrefiringWeights.hpp>

#include <mensura/Config.hpp>
#include <mensura/Dataset.hpp>
#include <mensura/PileUpReader.hpp>
#include <mensura/TFileService.hpp>

#include <TH1.h>
#include <TTree.h>

#include <filesystem>
#include <map>
#include <memory>


/**
 * \brief Stores event weights specific to data-taking periods
 *
 * The following weights are included:
 *  - integrated luminosity (in 1/pb),
 *  - pileup profile,
 *  - L1T prefiring weights (if a plugin that computes them is specified).
 * 
 * When computing the pileup weight, this plugin first tries to find the simulation profile for the
 * current data set, based on its ID, within the directory specified in the configuration file. Only
 * if this fails the default simulation profile will be used. This allows to correct for buggy
 * pileup profiles used in production of some MC samples. No systematic uncertainty is evaluated.
 *
 * This plugin must only be run on simulation.
 */
class PeriodWeights: public AnalysisPlugin
{
private:
    /// An aggregate of period-specific data
    struct Period
    {
        Period() noexcept;

        /// Integrated luminosity, in 1/pb
        double luminosity;

        /// Pileup profile in data
        std::unique_ptr<TH1> dataPileupProfile;

        /**
         * \brief Main weight for the current period
         *
         * This is used as a buffer for the output tree.
         */
        mutable Float_t weight;

        /// Period index used to access prefiring weights
        unsigned index;

        /**
         * \brief Variation in prefiring weights for the current period
         *
         * Used as buffers for the output tree. These weights are relative with respect to the
         * nominal one.
         */
        mutable Float_t prefiringWeightSyst[2];
    };

public:
    /**
     * \brief Constructor
     *
     * \param[in] name        Name to identify this plugin.
     * \param[in] configPath  Path to JSON configuration for this plugin.
     * \param[in] trigger     Name of selected trigger.
     */
    PeriodWeights(std::string const &name, std::string const &configPath,
      std::string const &trigger);
    
public:
    /**
     * \brief Saves pointers to required plugins and services and sets up output tree
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &dataset) override;
    
    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual PeriodWeights *Clone() const override;

    /// Specifies name of L1TPrefiringWeights plugin
    void SetPrefiringWeightPlugin(std::string const &name);

    /**
     * \brief Specifies the name for the output tree
     * 
     * Can also include the name of the in-file directory. By default the name of the plugin is used
     * for the tree.
     */
    void SetTreeName(std::string const &name);
    
private:
    /// Fills map \ref periods
    void ConstructPeriods();

    /**
     * \brief Computes variables and fills the output tree
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;

    /**
     * \brief Reads pileup profile from given file
     *
     * The path is relative and resolved with respect to \ref profilesDir. The profile is normalized
     * to a unit integral. It is owned by the caller.
     */
    TH1 *ReadProfile(std::filesystem::path path);
    
private:
    /// Parsed configuration
    Config config;

    /// Directory with pileup profiles
    std::filesystem::path profilesDir;

    /// Name of the requested trigger
    std::string triggerName;

    /**
     * \brief Period-specific details
     *
     * The key of the map is the period label.
     */
    std::map<std::string, Period> periods;

    /// Name of TFileService
    std::string fileServiceName;
    
    /// Non-owning pointer to TFileService
    TFileService const *fileService;
    
    /// Name of a plugin that reads information about pile-up
    std::string puPluginName;
    
    /// Non-owning pointer to a plugin that reads information about pile-up
    PileUpReader const *puPlugin;

    /// Name of a plugin that computes L1T prefiring weights
    std::string prefiringPluginName;

    /// Non-owning pointer to a plugin that computes L1T prefiring weights
    L1TPrefiringWeights const *prefiringPlugin;
    
    /// Name of the output tree and its in-file directory
    std::string treeName, directoryName;

    /**
     * \brief Pileup profile in simulation for current data set
     *
     * Normalized to a unit integral.
     */
    std::unique_ptr<TH1> simPileupProfile;
    
    /// Non-owning pointer to output tree
    TTree *tree;
};

