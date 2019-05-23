#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <mensura/Dataset.hpp>
#include <mensura/TFileService.hpp>
#include <mensura/PECReader/PECGeneratorReader.hpp>

#include <TTree.h>


/**
 * \brief Stores generator-level event weights
 *
 * If a generator reader is provided (with method \ref SetGeneratorReader), the full generator
 * weight includes the nominal raw generator-level weight. Otherwise this raw weight is assumed to
 * be 1. In any case the full weight includes the data set weight (Dataset::GetWeight).
 *
 * If a generator reader is provided, the weights for the variations in the renormalization and
 * factorization scales in the matrix element are also saved.
 *
 * The nominal weight, "WeightGen" must always be applied. Any other weights stored by this plugin
 * must be applied on top of this nominal weight. Whenever a branch represents a variation, the
 * corresponding weights are stored in an array of length 2, first the weight for the "up"
 * variation, then for the "down" one.
 *
 * This plugin must only be run on simulation.
 */
class GenWeights: public AnalysisPlugin
{
public:
    /// Constructs a plugin with the given name
    GenWeights(std::string const &name);
    
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
    virtual GenWeights *Clone() const override;

    /// Specifies the name of a PECGeneratorReader plugin
    void SetGeneratorReader(std::string const &name)
    {
        generatorPluginName = name;
    }
    
    /**
     * \brief Specifies the name for the output tree
     * 
     * Can also include the name of the in-file directory. By default the name of the plugin is used
     * for the tree.
     */
    void SetTreeName(std::string const &name);
    
private:
    /**
     * \brief Computes variables and fills the output tree
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of TFileService
    std::string fileServiceName;
    
    /// Non-owning pointer to TFileService
    TFileService const *fileService;
    
    /// Name of plugin that provides generator-level weight
    std::string generatorPluginName;
    
    /// Non-owning pointer to plugin that provides generator-level weight
    PECGeneratorReader const *generatorPlugin;
    
    /// Name of the output tree and its in-file directory
    std::string treeName, directoryName;

    /// Common weight from the data set
    double weightDataset;
    
    /// Non-owning pointer to output tree
    TTree *tree;
    
    // Output buffers
    Float_t bfWeightGen;

    Float_t bfWeightMERenorm[2], bfWeightMEFact[2];
};

