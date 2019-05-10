#pragma once

#include <mensura/AnalysisPlugin.hpp>

#include <string>


class JetMETReader;


/**
 * \class BalanceCalc
 * \brief Plugin to compute balance observables in the transverse plane
 * 
 * The MPF observable is computed from pt of the leading jet and the missing pt. The pt balance is
 * computed from jets above the given threshold in pt. The threshold is soft, meaning that jets
 * close to it are weighted down. If pt is below the nominal threshold, the weight is zero. Above
 * the threshold plus a user-defined margin, the weight is unity. In between the two boundaries the
 * weight changes in a smooth manner. This is done in order to make the pt balance in a given event
 * a continuous function of parameters of the L3Res correction.
 * 
 * An event is rejected if it contains no jets.
 */
class BalanceCalc: public AnalysisPlugin
{
public:
    /**
     * \brief Constructor from a name and definition of threshold for pt balance
     * 
     * \param name  Name for the plugin.
     * \param thresholdPtBalStart  Only jets above this pt threshold contribute to the pt balance.
     * \param thresholdPtBalEnd  If given, defines the range over which jets are included in the
     *     pt balance with a weight less than 1.
     */
    BalanceCalc(std::string const &name, double thresholdPtBalStart,
      double thresholdPtBalEnd = 0.);
    
    /// Short-cut for a construct with default name "BalanceCalc"
    BalanceCalc(double thresholdPtBalStart, double thresholdPtBalEnd = 0.);
    
public:
    /**
     * \brief Saves pointer to plugin that produces jets and missing pt
     * 
     * Reimplemented from Plugin.
     */
    virtual void BeginRun(Dataset const &) override;
    
    /**
     * \brief Creates a newly configured clone
     * 
     * Implemented from Plugin.
     */
    virtual BalanceCalc *Clone() const override;
    
    /// Returns value of MPF observable in current event
    double GetMPF() const;
    
    /// Returns value of pt balance observable in current event
    double GetPtBal() const;
    
private:
    /**
     * \brief Computes values of the balance observables
     * 
     * Implemented from Plugin.
     */
    virtual bool ProcessEvent() override;
    
    /**
     * \brief Computes jet weight for the computation of pt balance
     * 
     * \param pt  Transverse momentum of the jet.
     * \return  Weight in the range from 0. to 1.
     * 
     * Details are given in the documentation for the class and the constructor.
     */
    double WeightJet(double pt) const;
    
private:
    /// Threshold in pt to be used in computation of pt balance
    double thresholdPtBal;
    
    /// (Absolute) length of the turn-on part of the threshold
    double turnOnPtBal;
    
    /// Name of the plugin that produces jets
    std::string jetmetPluginName;
    
    /// Non-owning pointer to the plugin that produces jets
    JetMETReader const *jetmetPlugin;
    
    /// Values of balance observables in the current event
    double ptBal, mpf;
};
