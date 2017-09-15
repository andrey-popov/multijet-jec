/**
 * Fits jet correction using multijet data.
 */

#include <FitBase.hpp>
#include <Multijet.hpp>

#include <Minuit2/Minuit2Minimizer.h>
#include <Math/Functor.h>
#include <TMath.h>

#include <cmath>
#include <fstream>
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
    
    
    
    unsigned const nPars = lossFunc.GetNumParams();
    
    // Create minimizer
    ROOT::Minuit2::Minuit2Minimizer minimizer;
    ROOT::Math::Functor func(&lossFunc, &CombLossFunction::EvalRawInput, nPars);
    minimizer.SetFunction(func);
    minimizer.SetStrategy(2);   // high quality
    minimizer.SetErrorDef(1.);  // error level for a chi2 function
    minimizer.SetPrintLevel(3);
    
    
    // Initial point
    for (unsigned i = 0; i < nPars; ++i)
        minimizer.SetVariable(i, "p"s + to_string(i), 0., 1e-2);
    
    
    // Run minimization
    minimizer.Minimize();
    
    
    // Print results
    cout << "\n\n\e[1mSummary\e[0m:\n";
    cout << "  Status: " << minimizer.Status() << '\n';
    cout << "  Covariance matrix status: " << minimizer.CovMatrixStatus() << '\n';
    cout << "  Minimal value: " << minimizer.MinValue() << '\n';
    cout << "  NDF: " << lossFunc.GetNDF() << '\n';
    cout << "  p-value: " << TMath::Prob(minimizer.MinValue(), lossFunc.GetNDF()) << '\n';
    
    double const *results = minimizer.X();
    double const *errors = minimizer.Errors();
    cout << "  Parameters:\n";
    
    for (unsigned i = 0; i < nPars; ++i)
        cout << "    " << minimizer.VariableName(i) << ":  " << results[i] << " +- " <<
          errors[i] << "\n";
    
    
    // Save fit results in a text file
    string const resFileName("fit.out");
    ofstream resFile(resFileName);
    
    resFile << "# Fitted parameters\n";
    
    for (unsigned i = 0; i < nPars; ++i)
        resFile << results[i] << " ";
    
    resFile << "\n\n# Covariance matrix:\n";
    
    for (unsigned i = 0; i < nPars; ++i)
    {
        for (unsigned j = 0; j < nPars; ++j)
            resFile << minimizer.CovMatrix(i, j) << " ";
        
        resFile << '\n';
    }
    
    resFile.close();
    
    
    cout << "\nResults saved to file \"" << resFileName << "\".\n";
    
    
    return EXIT_SUCCESS;
}
