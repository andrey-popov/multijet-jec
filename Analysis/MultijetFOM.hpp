#pragma once

#include <TFile.h>
#include <TH1D.h>
#include <TH3D.h>

#include <cmath>
#include <map>
#include <memory>
#include <utility>



/**
 * \class L3Corr
 * \brief Functor that computes L3 residual jet energy correction
 */
class L3Corr
{
public:
    /// Supported functional forms
    enum class Type
    {
        Linear,
        LogLinear,
        Polynomial
    };
    
public:
    /// Constructor
    L3Corr(Type type);
    
public:
    /**
     * \brief Computes correction at the given pt using provided parameteres
     * 
     * The pt is supposed to be L2-corrected.
     */
    double operator()(double pt, double const *pars) const;
    
    /// Returns number of parameters
    unsigned GetNumParameters() const;
    
    /// Changes the functional form of the correction
    void SetType(Type type);
    
private:
    /// Computes linear correction
    double Linear(double pt, double const *pars) const;
    
    /// Computes log-linear correction
    double LogLinear(double pt, double const *pars) const;
    
    /// Computes correction given by a polymonial function
    double Polynomial(double pt, double const *pars) const;
    
private:
    /// Selected functional form
    Type type;
    
    /// Pointer to the function implementing the correction
    double (L3Corr::*func)(double, double const *) const;
    
    /// Chosen degree for the polynomial function
    unsigned degree;
    
    /// Scale at which the correction must evaluate to unity
    double refPt;
};


/**
 * \class MultijetFOM
 * \brief A class to compute FOM to be minimized in fitting of L3 corrections
 */
class MultijetFOM
{
public:
    /// Constructor from a file with inputs for global fit and function implementing L3 correction
    MultijetFOM(std::string const &inputFileName, L3Corr const &corrector);
    
    MultijetFOM(MultijetFOM const &) = delete;
    MultijetFOM &operator=(MultijetFOM const &) = delete;
    
public:
    /// Compute FOM for the given set of parameters
    double operator()(double const *pars) const;
    
    /// Returns number of parameters
    unsigned GetDim() const;
    
private:
    /// Functor that computes L3 correction
    L3Corr const &corrector;
    
    /**
     * \brief Histogram with data distribution
     * 
     * Coordinates are ptLeadProj, MJB, F.
     */
    std::unique_ptr<TH3I> histData3D;
    
    /// Mean MJB as a function of ptLeadProj in simulation
    std::unique_ptr<TH1> histSim;
    
    /**
     * \brief Ranges of bins in ptLeadProj in the data histogram corresponding to wide bins in
     * the histogram in simulation
     * 
     * The pair gives the first and the last bin included in the range.
     */
    std::map<unsigned, std::pair<unsigned, unsigned>> ptLeadBinMap;
};
