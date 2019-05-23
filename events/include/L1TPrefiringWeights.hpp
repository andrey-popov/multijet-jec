#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <mensura/JetMETReader.hpp>

#include <TH1.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>


/**
 * \brief Computes L1T prefiring weights
 *
 * This plugin should be run on simulation only. For each event it computes probabilities that it
 * has not been self-vetoed because of the L1T prefiring. The probabilities are computed for all
 * requested data-taking periods. Systematic variations are also provided.
 */
class L1TPrefiringWeights: public AnalysisPlugin
{
private:
    /// Auxiliary class to compute weight for a single data-taking period
    class WeightCalc
    {
    private:
        /// Supported systematic variations for prefiring probabilities
        enum class Variation
        {
            Nominal,
            Up,
            Down
        };

    public:
        /// Constructor from a prefiring map
        WeightCalc(TH1 *prefiringMap_):
            prefiringMap{prefiringMap_}
        {}

    public:
        /// Computes event weights with given jets
        std::array<double, 3> ComputeWeights(std::vector<Jet> const &jets) const;

    private:
        /// Computes event weight for given systematic variation
        double ComputeWeight(std::vector<Jet> const &jets, Variation variation) const;

        /// Returns prefiring probability for given jet
        double GetPrefiringProbability(Jet const &jet, Variation variation) const;

    private:
        /**
         * \brief Prefiring map
         *
         * Parameterized with (eta, pt) of jets; overflows in pt are filled properly.
         */
        std::unique_ptr<TH1> prefiringMap;
    };

public:
    /**
     * \brief Constructor
     *
     * \param[in] name        Name for this plugin.
     * \param[in] configPath  Path to JSON configuration for period weights (same as for plugin
     *   PeriodWeights).
     */
    L1TPrefiringWeights(std::string const &name, std::string const &configPath);

    /**
     * \brief Version of L1TPrefiringWeights(std::string const &, std::string const &) that uses
     * default plugin name "L1TPrefiringWeights"
     */
    L1TPrefiringWeights(std::string const &configPath);

public:
    /**
     * \brief Saves pointers to required plugins and services and sets up output tree
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &) override;

    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual L1TPrefiringWeights *Clone() const override;

    /**
     * \brief Returns index to which the given period label corresponds
     *
     * This allows to use save the index and then use the more efficient version of \ref GetWeight.
     */
    unsigned FindPeriodIndex(std::string const &periodLabel) const;

    /**
     * \brief Returns L1T prefiring weights for the period with the given index
     *
     * The weights are given in the following order: nominal, increased prefiring probability,
     * decreased prefiring probability.
     */
    std::array<double, 3> GetWeights(unsigned periodIndex) const
    {
        return cachedWeights.at(periodIndex);
    }

    /// Version of GetWeights(unsigned) that identifies a period by its label
    std::array<double, 3> GetWeight(std::string const &periodLabel) const
    {
        return GetWeights(FindPeriodIndex(periodLabel));
    }

private:
    /// Construct WeightCalc objects for all periods
    void BuildCalcs(std::string const &configPath);
    
    /// Computes prefiring weights for the current event
    virtual bool ProcessEvent() override;

private:
    /// Name of a plugin that produces jets
    std::string jetmetPluginName;
    
    /// Non-owning pointer to a plugin that produces jets
    JetMETReader const *jetmetPlugin;

    /**
     * \brief Objects to compute prefiring weights in different data-taking periods
     *
     * Shared among all clones of this plugin.
     */
    std::shared_ptr<std::vector<WeightCalc>> calcs;

    /**
     * \brief Maps period labels into indices of \ref calcs
     *
     * Shared among all clones of this plugin.
     */
    std::shared_ptr<std::map<std::string, unsigned>> periodLabelMap;

    /// Cached results for WeightCalc::GetWeights for all data-taking periods
    std::vector<std::array<double, 3>> cachedWeights;
};

