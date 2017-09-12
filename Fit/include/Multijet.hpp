#pragma once

#include <FitBase.hpp>

#include <TH1.h>
#include <TH2.h>
#include <TProfile.h>

#include <memory>
#include <string>
#include <vector>


struct FracBin;


/**
 * \class Multijet
 * \brief Implements computation of the deviation of data from expectation in the multijet analysis
 * 
 * The deviation is computed as a chi^2 distance,
 *   chi^2 = sum_i (B^{Data}_i - B^{Sim}_i)^2 / (sigma^{Data}_i^2 + sigma^{Sim}_i^2),
 * where B_i is the mean balance observable in bin i in pt of the leading jet and sigma_i is its
 * statistical uncertainty. In data the mean balance observable is recomputed for the given jet
 * correction following the method described in [1-2].
 * [1] https://indico.cern.ch/event/646599/#50-on-the-way-to-an-updated-mu
 * [2] https://indico.cern.ch/event/656050/#65-comparison-of-different-app
 */
class Multijet: public DeviationBase
{
public:
    /// Supported methods of computation
    enum class Method
    {
        PtBal,
        MPF
    };
    
private:
    /// Auxiliary structure to aggregate data related to a single trigger bin
    struct TriggerBin
    {
        /**
         * \brief Binning in pt of the leading jet in data
         * 
         * The same binning is used for all data histograms and profiles.
         */
        std::vector<double> binning;
        
        /**
         * \brief Profiles of the balance observable in data and simulation
         * 
         * Binning of the profile in simulation defines bins to compute chi^2.
         */
        std::unique_ptr<TProfile> balProfile, simBalProfile;
        
        /// Distribution of pt of the leading jet in data
        std::unique_ptr<TH1> ptLead;
        
        /**
         * \brief Profile of pt of the leading jet in data
         * 
         * Used to obtain true mean pt in each bin.
         */
        std::unique_ptr<TProfile> ptLeadProfile;
        
        /// Sum of projections of pt of jets in bins of pt of the leading and other jets
        std::unique_ptr<TH2> ptJetSumProj;
        
        /**
         * \brief Squared uncertainty on the difference between mean balance observables in data
         * and simulation
         * 
         * Computed in the binning of simBalProfile.
         */
        std::vector<double> totalUnc2;
    };
        
public:
    /// Constructor
    Multijet(std::string const &fileName, Method method);
    
public:
    /**
     * \brief Returns dimensionality of the deviation
     * 
     * Implemented from DeviationBase.
     */
    virtual unsigned GetDim() const override;
    
    /**
     * \brief Evaluates the deviation with the given jet corrector and set of nuisances
     * 
     * Implemented from DeviationBase.
     */
    virtual double Eval(JetCorrBase const &corrector, Nuisances const &) const override;
    
private:
    /// Recomputes MPF in data for given trigger bin, 2D pt window, and jet correction
    static double ComputeMPF(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
      FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector);
    
    /// Recomputes pt balance in data for given trigger bin, 2D pt window, and jet correction
    static double ComputePtBal(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
      FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector);
        
private:
    /// Method of computation
    Method method;
    
    /// Inputs for different trigger bins
    std::vector<TriggerBin> triggerBins;
    
    /// Jet pt threshold
    double minPt;
    
    /// Dimensionality of the deviation
    unsigned dimensionality;
};
