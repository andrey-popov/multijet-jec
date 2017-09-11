#pragma once

#include <vector>


/**
 * \class JetCorrBase
 * \brief Base class to for a jet correction
 * 
 * The correction is multiplicative. Its parameters are stored as data members.
 */
class JetCorrBase
{
public:
    JetCorrBase(unsigned numParams);
    
public:
    /// Returns number of parameters of the correction
    unsigned GetNumParams() const;
    
    /**
     * \brief Evaluates the correction for the given jet pt
     * 
     * To be implemented in a derived class.
     */
    virtual double Eval(double pt) const = 0;
    
    /**
     * \brief Updates parameters of the correction
     * 
     * Throws an exception if the given number of parameters does not match the expected one.
     */
    void SetParams(std::vector<double> const &newParams);
    
    /**
     * \brief Inverts jet correction
     * 
     * Returns uncorrected pt such that ptUncorr * corr(ptUncorr) recovers the given corrected pt.
     * The computation is done iteratively and stops when the corrected pt is reproduced with the
     * specified relative tolerance.
     */
    virtual double UndoCorr(double pt, double tolerance = 1e-10) const;
    
protected:
    /// Current parameters of the correction
    std::vector<double> parameters;
};


/**
 * \struct NuisancesBase
 * \brief (Trivial) base class for a set of nuisance parameters
 * 
 * A derived class is expected to implement nuisance parameters as publicly available data members.
 */
struct NuisancesBase
{};


/// An instance of NuisancesBase to be used when no dependence on nuisances is implemented
extern NuisancesBase const dummyNuisances;


/**
 * \class DeviationBase
 * \brief Base class to describe a loss function for fitting of the jet correction
 * 
 * The loss function is expected to quantify the deviation of data with the current jet correction
 * from simulation. This deviation is to be minimized during the fit for parameters of the jet
 * correction. A derived class should implement a single analysis.
 */
class DeviationBase
{
public:
    /**
     * \brief Returns dimensionality of the deviation
     * 
     * The number of degrees of freedom is this number minus the number of fitted parameters. For a
     * chi^2 measure, the dimensionality is the number of individual chi^2 terms in the sum.
     * 
     * To be implemented in a derived class.
     */
    virtual unsigned GetDim() const = 0;
    
    /**
     * \brief Evaluates the loss function with the given jet corrector and set of nuisances
     * 
     * To be implemented in a derived class.
     */
    virtual double Eval(JetCorrBase const &corrector, NuisancesBase const &nuisances) const = 0;
};
