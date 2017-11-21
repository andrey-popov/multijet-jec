#pragma once

#include <mensura/core/AnalysisPlugin.hpp>

#include <string>
#include <vector>


class EventIDReader;
class JetMETReader;


/**
 * \class EtaPhiFilter
 * \brief Applies filtering based on an (eta, phi) map
 * 
 * An event is rejected if at least one jet with pt above a given threshold falls inside an
 * excluded region in the (eta, phi) plane, for the given run range. This plugin relies on the
 * presence of a EventIDReader with a default name "InputData" and a JetMETReader with a default
 * name "JetMET".
 */
class EtaPhiFilter: public AnalysisPlugin
{
private:
    /// Auxiliary structure that defines a rectangular region in (eta, phi) for a run range
    struct Region
    {
        /// Constructor with a complete initialization
        Region(unsigned long minRun, unsigned long maxRun, double minEta, double maxEta,
          double minPhi, double maxPhi);
        
        /**
         * \brief Checks if given point (eta, phi) is included in the region
         * 
         * The phi coordinate must be in the range [-pi, pi].
         */
        bool InEtaPhi(double eta, double phi) const;
        
        /// Checks if given run is within the targeted run range
        bool InRunRange(unsigned long run) const;
        
        /**
         * \brief Targeted run range
         * 
         * Boundaries are included.
         */
        unsigned long minRun, maxRun;
        
        /**
         * \brief Range in eta
         * 
         * Expect minEta < maxEta.
         */
        double minEta, maxEta;
        
        /**
         * \brief Range in phi
         * 
         * Stored in a canonical form in which 0 <= minPhi < maxPhi.
         */
        double minPhi, maxPhi;
    };
    
public:
    /// Creates a new instance with the given pt threshold
    EtaPhiFilter(std::string const &name, double minPt);
    
    /// A short-cut for the above version with a default name "EtaPhiFilter"
    EtaPhiFilter(double minPt);
    
public:
    /**
     * \brief Registers a "bad" region in (eta, phi)
     * 
     * The region is a rectangle defined by (startEta, endEta, startPhi, endPhi). It is assumed
     * that startEta < endEta. The phi coordinates must be in the range (-pi, pi]. Along the phi
     * coordinate the region spans in the counter-clockwise direction starting from angle startPhi
     * and ending at endPhi; switching the two argument would select the complementary part. The
     * filtering is restricted to the specified run range (boundaries are included).
     */
    void AddRegion(unsigned long minRun, unsigned long maxRun, double startEta, double endEta,
      double startPhi, double endPhi);
    
    /**
     * \brief Saves pointer to the jet reader
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &) override;
    
    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual Plugin *Clone() const override;
    
private:
    /**
     * \brief Performs selection on the leading jet
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
private:
    /// Name of the plugin that provides access to event ID
    std::string eventIDPluginName;
    
    /// Non-owning pointer to the plugin that provides access to event ID
    EventIDReader const *eventIDPlugin;
    
    /// Name of the plugin that produces jets
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets
    JetMETReader const *jetmetPlugin;
    
    /// Minimal jet pt
    double minPt;
    
    /// Registered regions in (eta, phi)
    std::vector<Region> regions;
    
    /**
     * \brief Pointers to regions selected for the current event
     * 
     * Placed in the class definition in order to avoid memory allocation for each event.
     */
    mutable std::vector<Region const *> selectedRegions;
};
