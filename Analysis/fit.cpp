/**
 * Fits JEC using multijet data.
 */

#include "MultijetFOM.hpp"

#include <Minuit2/Minuit2Minimizer.h>
#include <Math/Functor.h>

#include <iostream>
#include <string>


int main(int argc, char **argv)
{
    using namespace std;
    
    // Parse arguments
    if (argc < 2 or argc > 3)
    {
        cerr << "Usage: fit inputFile.root [fitFunction]\n";
        return EXIT_FAILURE;
    }
    
    string const inputFileName(argv[1]);
    L3Corr::Type functionType = L3Corr::Type::Linear;
    
    if (argc > 2)
    {
        string const functionTypeText(argv[2]);
        
        if (functionTypeText == "linear")
            functionType = L3Corr::Type::Linear;
        else if (functionTypeText == "loglinear")
            functionType = L3Corr::Type::LogLinear;
        else
        {
            cerr << "Cannot recongnize function type \"" << functionTypeText << "\".\n";
            return EXIT_FAILURE;
        }
    }
    
    
    // Create a corrector function and an object to compute FOM for minimization
    L3Corr corrector(functionType);
    MultijetFOM fom(inputFileName, corrector);
    
    
    // Create minimizer
    ROOT::Minuit2::Minuit2Minimizer minimizer;
    ROOT::Math::Functor func(&fom, &MultijetFOM::operator(), fom.GetDim());
    minimizer.SetFunction(func);
    minimizer.SetStrategy(2);   // high quality
    minimizer.SetErrorDef(1.);  // error level for a chi2 function
    minimizer.SetPrintLevel(3);
    
    
    // Initial point
    for (unsigned i = 0; i < fom.GetDim(); ++i)
        minimizer.SetVariable(i, "p"s + to_string(i + 1), 0., 1e-3);
    
    
    // Run minimization
    minimizer.Minimize();
    double const *results = minimizer.X();
    
    
    // Print results
    cout << "Fit results:\n";
    cout << " Status: " << minimizer.Status() << '\n';
    cout << " Parameters: " << results[0];
    
    for (unsigned i = 1; i < fom.GetDim(); ++i)
        cout << ", " << results[i];
    
    cout << endl;
    
    
    return EXIT_SUCCESS;
}
