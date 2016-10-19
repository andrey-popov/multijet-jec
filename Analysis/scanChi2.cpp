/**
 * Performs a 1D scan of multijet chi2.
 */

#include "MultijetFOM.hpp"

#include <TCanvas.h>
#include <TGaxis.h>
#include <TGraph.h>
#include <TStyle.h>

#include <string>


using namespace std::string_literals;


void Scan(std::string const &fileName, L3Corr::Type funcType, double min, double max,
  unsigned nSteps, std::string const &figName)
{
    // Run the scan and save results to a graph
    L3Corr corrector(funcType);
    MultijetFOM fom(fileName, corrector);
    double pars[1];
    
    TGraph graph(nSteps);
    
    double x = min;
    double const step = (max - min) / nSteps;
    
    for (unsigned i = 0; i < nSteps; ++i)
    {
        pars[0] = x;
        graph.SetPoint(i, x, fom(pars));
        x += step;
    }
    
    
    // Draw results
    TCanvas canvas("canvas", "", 1500, 1000);
    canvas.SetTicks();
    canvas.SetLogy();
    
    graph.SetTitle(";Parameter;#chi^{2}");
    graph.Draw("al");
    
    canvas.Print(("fig/"s + figName + ".pdf").c_str());
    canvas.Print(("fig/"s + figName + ".root").c_str());
}


int main()
{
    // Set ROOT style
    gStyle->SetStripDecimals(false);
    TGaxis::SetMaxDigits(3);
    gStyle->SetTitleFont(42);
    gStyle->SetTitleFontSize(0.04);
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetTitleXOffset(0.9);
    gStyle->SetTitleYOffset(0.9);
    gStyle->SetTitleSize(0.045, "XYZ");
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetLabelOffset(0.007, "XYZ");
    gStyle->SetLabelSize(0.04, "XYZ");
    gStyle->SetNdivisions(508, "XYZ");
    
    // Run the scans
    Scan("multijet_linear.root", L3Corr::Type::Linear, -5e-3, 5e-3, 200, "scanChi2_linear");
    Scan("multijet_linear.root", L3Corr::Type::Linear, -1e-4, 1e-4, 200, "scanChi2_linear_zoom");
    Scan("multijet_loglinear.root", L3Corr::Type::LogLinear, -0.1, 0.1, 200, "scanChi2_loglinear");
    Scan("multijet_loglinear.root", L3Corr::Type::LogLinear, 0., 1e-2, 200, "scanChi2_loglinear_zoom");
    
    return EXIT_SUCCESS;
}
