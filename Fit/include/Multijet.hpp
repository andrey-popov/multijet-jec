#pragma once

#include <FitBase.hpp>

#include <TH1.h>
#include <TH2.h>
#include <TProfile.h>

#include <memory>
#include <string>
#include <vector>


struct FracBin;


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
        
private:
    Method method;
    std::vector<TriggerBin> triggerBins;
    double minPt;
    unsigned dimensionality;
};
