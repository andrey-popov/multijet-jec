#pragma once

#include <FitBase.hpp>

#include <TH1.h>
#include <TH2.h>
#include <TProfile.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>


class Multijet: public DeviationBase
{
public:
    enum class Method
    {
        PtBal,
        MPF
    };
    
private:
    struct TriggerBin
    {
        std::vector<double> binning;
        std::unique_ptr<TProfile> simBalProfile, balProfile;
        std::unique_ptr<TH1> ptLead;
        std::unique_ptr<TProfile> ptLeadProfile;
        std::unique_ptr<TH2> ptJetSumProj;
        std::vector<double> totalUnc2;
    };
    
    /**
     * \struct FracBin
     * \brief Auxiliary POD to describe a bin with an attached fraction
     * 
     * Used for partly included bins or to describe a relative position inside a bin.
     */
    struct FracBin
    {
        /// Index of the bin
        unsigned index;
        
        /// Fraction of the bin
        double frac;
    };
    
    using BinMap = std::map<unsigned, std::array<FracBin, 2>>;
    
public:
    Multijet(std::string const &fileName, Method method, double minPt);
    
public:
    virtual unsigned GetDim() const override;
    
    virtual double Eval(JetCorrBase const &corrector, NuisancesBase const &) const override;
    
private:
    static double ComputeMPF(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
      FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector);
    
    static double ComputePtBal(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
      FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector);
    
    /**
     * \brief Constructs a mapping from one binning to another
     * 
     * Constructs a mapping from the source binning to the target one. If bin edges do not align,
     * performs and interpolation. The full range of the target binning must be included in the
     * range of the source one (i.e. no extrapolation is performed). Both vectors must be sorted;
     * this condition is not verified. Normally the target binning is coarser so that source bins
     * need to be merged, but this is not required, and a single source bin can be mapped to
     * multiple bins of the target binning.
     * 
     * Returns a map from indices of target bins to ranges of bins of the source binning. The
     * ranges are represented by pairs of FracBin objects, which give indices of boundary bins of
     * the range and their inclusion fractions. If both ends of the range have the same index (i.e.
     * a single source bin has been mapped into multiple target bins), the inclusion fraction for
     * the upper boundary is set to zero. Bins are numbered such that the underflow bin is assigned
     * index 0.
     */
    static BinMap MapBinning(std::vector<double> const &source, std::vector<double> const &target);
    
private:
    Method method;
    std::vector<TriggerBin> triggerBins;
    double minPt;
    unsigned dimensionality;
};
