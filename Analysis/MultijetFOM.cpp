#include "MultijetFOM.hpp"


L3Corr::L3Corr(Type type_):
    type(type_),
    refPt(1000.)
{
    SetType(type);
}


double L3Corr::operator()(double pt, double const *pars) const
{
    return (this->*func)(pt, pars);
}


unsigned L3Corr::GetNumParameters() const
{
    switch (type)
    {
        case Type::Linear:
            return 1;
        
        case Type::LogLinear:
            return 1;
        
        case Type::Polynomial:
            return degree;
        
        default:
            return 0;
    }
}


void L3Corr::SetType(Type type_)
{
    type = type_;
    
    switch (type)
    {
        case Type::Linear:
            func = &L3Corr::Linear;
            degree = 1;
            break;
        
        case Type::LogLinear:
            func = &L3Corr::LogLinear;
            break;
        
        default:
            std::ostringstream message;
            message << "L3Corr::SetType: Undefined type " << int(type) << ".";
            throw std::runtime_error(message.str());
    }
}


double L3Corr::Linear(double pt, double const *pars) const
{
    return 1. + pars[0] * (pt - refPt);
}


double L3Corr::LogLinear(double pt, double const *pars) const
{
    return 1. + pars[0] * std::log(pt / refPt);
}


double L3Corr::Polynomial(double pt, double const *pars) const
{
    double const x = pt - refPt;
    double sum = 1.;
    double xn = 1.;
    
    for (unsigned i = 0; i < degree; ++i)
    {
        xn *= x;
        sum += pars[i] * xn;
    }
    
    return sum;
}



MultijetFOM::MultijetFOM(std::string const &inputFileName, L3Corr const &corrector_):
    corrector(corrector_)
{
    // Read input histograms from the file
    TFile inputFile(inputFileName.c_str());
    
    histData3D.reset(dynamic_cast<TH3I *>(inputFile.Get("MJB_Pt30/Data")));
    histSim.reset(dynamic_cast<TH1 *>(inputFile.Get("MJB_Pt30/Simulation")));
    
    histData3D->SetDirectory(nullptr);
    histSim->SetDirectory(nullptr);
    inputFile.Close();
    
    
    // Build a correspondence between wide bins of histogram with simulation and fine bins in data
    unsigned bin = 1;
    auto const *fineAxis = histData3D->GetXaxis();
    
    for (unsigned wideBin = 1; wideBin <= unsigned(histSim->GetNbinsX()); ++wideBin)
    {
        double const wideBinLowerEdge = histSim->GetBinLowEdge(wideBin);
        double const wideBinUpperEdge = histSim->GetBinLowEdge(wideBin + 1);
        
        while (fineAxis->GetBinCenter(bin) < wideBinLowerEdge)
            ++bin;
        
        unsigned const firstBin = bin;
        
        while (fineAxis->GetBinCenter(bin) < wideBinUpperEdge)
            ++bin;
        
        unsigned const lastBin = bin - 1;
        
        ptLeadBinMap.emplace(std::piecewise_construct, std::forward_as_tuple(wideBin),
          std::forward_as_tuple(firstBin, lastBin));
    }
}


double MultijetFOM::operator()(double const *pars) const
{
    double sumChi2 = 0.;
    
    for (unsigned bin = 1; bin <= unsigned(histSim->GetNbinsX()); ++bin)
    {
        // Compute recorrected mean MJB in data
        unsigned long nEvents = 0;
        double sumMJB = 0.;
        double sumMJB2 = 0.;
        
        auto const &rangePtLeadProj = ptLeadBinMap.at(bin);
        
        for (unsigned binPtLeadProj = rangePtLeadProj.first;
          binPtLeadProj <= rangePtLeadProj.second; ++binPtLeadProj)
        {
            // Projection of ptLead in this bin and JEC evaluated at this scale
            double const ptLeadProj = histData3D->GetXaxis()->GetBinCenter(binPtLeadProj);
            double const jecPtLeadProj = corrector(ptLeadProj, pars);
            
            for (unsigned binMJB = 1; binMJB <= unsigned(histData3D->GetNbinsY()); ++binMJB)
            {
                double const MJB = histData3D->GetYaxis()->GetBinCenter(binMJB);
                
                for (unsigned binF = 1; binF <= unsigned(histData3D->GetNbinsZ()); ++binF)
                {
                    double const F = histData3D->GetZaxis()->GetBinCenter(binF);
                    
                    // Apply current JEC to MJB in this bin and add it to the accumulators
                    double const correctedMJB = MJB *
                      jecPtLeadProj / corrector(F * ptLeadProj / MJB, pars);
                    
                    unsigned long const n = histData3D->GetBinContent(binPtLeadProj, binMJB, binF);
                    nEvents += n;
                    sumMJB += correctedMJB * n;
                    sumMJB2 += std::pow(correctedMJB, 2) * n;
                }
            }
        }
        
        double const meanMJBData = sumMJB / nEvents;
        double const meanMJBDataUnc2 = (sumMJB2 / nEvents - std::pow(meanMJBData, 2)) / nEvents;
        
        
        // Read mean MJB and its (squared) uncertainty in simulation
        double const meanMJBSim = histSim->GetBinContent(bin);
        double const meanMJBSimUnc2 = std::pow(histSim->GetBinError(bin), 2);
        
        
        // Add the term for the current bin to the full chi2
        sumChi2 += std::pow(meanMJBData - meanMJBSim, 2) / (meanMJBDataUnc2 + meanMJBSimUnc2);
    }
    
    return sumChi2;
}


unsigned MultijetFOM::GetDim() const
{
    return corrector.GetNumParameters();
}
