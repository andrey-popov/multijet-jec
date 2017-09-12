/**
 * Fits jet correction using multijet data.
 */

#include <FitBase.hpp>
#include <Multijet.hpp>

#include <Minuit2/Minuit2Minimizer.h>
#include <Math/Functor.h>

#include <iostream>
#include <memory>
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


int main(int argc, char **argv)
{
    using namespace std;
    
    
    // Parse arguments
    if (argc != 2)
    {
        cerr << "Usage: fit inputFile.root\n";
        return EXIT_FAILURE;
    }
    
    
    // Construct an object to evaluate the loss function
    auto jetCorr = make_unique<JetCorr>();
    CombLossFunction lossFunc(move(jetCorr));
    
    Multijet measurementMultijet(argv[1], Multijet::Method::PtBal);
    lossFunc.AddMeasurement(&measurementMultijet);
    
    
    
    // Create minimizer
    ROOT::Minuit2::Minuit2Minimizer minimizer;
    ROOT::Math::Functor func(&lossFunc, &CombLossFunction::EvalRawInput, lossFunc.GetNumParams());
    minimizer.SetFunction(func);
    minimizer.SetStrategy(2);   // high quality
    minimizer.SetErrorDef(1.);  // error level for a chi2 function
    minimizer.SetPrintLevel(3);
    
    
    // Initial point
    for (unsigned i = 0; i < lossFunc.GetNumParams(); ++i)
        minimizer.SetVariable(i, "p"s + to_string(i + 1), 0., 1e-3);
    
    
    // Run minimization
    minimizer.Minimize();
    double const *results = minimizer.X();
    
    
    // Print results
    cout << "Fit results:\n";
    cout << " Status: " << minimizer.Status() << '\n';
    cout << " Parameters: " << results[0];
    
    for (unsigned i = 1; i < lossFunc.GetNumParams(); ++i)
        cout << ", " << results[i];
    
    cout << endl;
    
    
    return EXIT_SUCCESS;
}
