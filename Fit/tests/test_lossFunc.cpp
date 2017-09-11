/**
 * A unit test to check evaluation of the multijet loss function.
 * 
 * This is currently a draft. Will put a small ROOT file into the repository and check computed
 * values of the loss function against a reference.
 */

#include <FitBase.hpp>
#include <Multijet.hpp>

#include <iostream>
#include <string>


class JetCorr: public JetCorrBase
{
public:
    JetCorr();
    
public:
    virtual double Eval(double pt) const override;
};


JetCorr::JetCorr():
    JetCorrBase(1)
{}


double JetCorr::Eval(double pt) const
{
    double const b = 1.;
    double const ptmin = 15.;
    return 1. + parameters[0] * std::log(pt / ptmin) +
      parameters[0] / b * (std::pow(pt / ptmin, -b) - 1);
}


int main()
{
    JetCorr jetCorr;
    NuisancesBase dummyNuisances;
    
    std::string inputFile("~/workspace/Analyses/JetMET/2017.09.07_New-method-real-setup/Analysis/multijet.root");
    
    std::cout << "Loss function for pt balancing with various jet corrections:\n";
    Multijet lossFuncPtBal(inputFile, Multijet::Method::PtBal, 30.);
    
    for (auto const &p: {-2e-2, -1e-2, -5e-3, 0., 5e-3, 1e-2, 2e-2})
    {
        jetCorr.SetParams({p});
        std::cout << "  " << lossFuncPtBal.Eval(jetCorr, dummyNuisances) << std::endl;
    }
    
    std::cout << "\nLoss function for MPF with various jet corrections:\n";
    Multijet lossFuncMPF(inputFile, Multijet::Method::MPF, 30.);
    
    for (auto const &p: {-2e-2, -1e-2, -5e-3, 0., 5e-3, 1e-2, 2e-2})
    {
        jetCorr.SetParams({p});
        std::cout << "  " << lossFuncMPF.Eval(jetCorr, dummyNuisances) << std::endl;
    }
    
    return EXIT_SUCCESS;
}
