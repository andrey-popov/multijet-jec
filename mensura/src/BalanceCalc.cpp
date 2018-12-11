#include <BalanceCalc.hpp>

#include <mensura/core/JetMETReader.hpp>

#include <cmath>
#include <sstream>
#include <stdexcept>


BalanceCalc::BalanceCalc(std::string const &name, double thresholdPtBalStart,
  double thresholdPtBalEnd):
    AnalysisPlugin(name),
    thresholdPtBal(thresholdPtBalStart),
    jetmetPluginName("JetMET")
{
    if (thresholdPtBalEnd <= 0. or thresholdPtBalStart == thresholdPtBalEnd)
        turnOnPtBal = 0.;
    else
    {
        if (thresholdPtBalEnd < thresholdPtBalStart)
        {
            std::ostringstream message;
            message << "BalanceCalc::BalanceCalc[" << name << "]: Wrong ordering in range (" <<
              thresholdPtBalStart << ", " << thresholdPtBalEnd << ").";
            throw std::runtime_error(message.str());
        }
        
        turnOnPtBal = thresholdPtBalEnd -  thresholdPtBalStart;
    }
}


BalanceCalc::BalanceCalc(double thresholdPtBalStart, double thresholdPtBalEnd):
    BalanceCalc("BalanceCalc", thresholdPtBalStart, thresholdPtBalEnd)
{}


void BalanceCalc::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


BalanceCalc *BalanceCalc::Clone() const
{
    return new BalanceCalc(*this);
}


double BalanceCalc::GetMPF() const
{
    return mpf;
}


double BalanceCalc::GetPtBal() const
{
    return ptBal;
}


bool BalanceCalc::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() < 1)
        return false;
    
    
    auto const p4Lead = jets[0].P4();
    auto const p4Miss = jetmetPlugin->GetMET().P4();
    mpf = 1. + (p4Miss.Px() * p4Lead.Px() + p4Miss.Py() * p4Lead.Py()) / std::pow(p4Lead.Pt(), 2);
    
    
    // Compute pt balance with a smooth threshold
    double s = 0.;
    
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        TLorentzVector const &p4 = jets[i].P4();
        
        if (p4.Pt() < thresholdPtBal)
        {
            // Jets are sorted in decreasing order in pt
            break;
        }
        
        s += p4.Pt() * std::cos(p4.Phi() - p4Lead.Phi()) * WeightJet(p4.Pt());
    }
    
    ptBal = -s / p4Lead.Pt();
    
    return true;
}


double BalanceCalc::WeightJet(double pt) const
{
    if (turnOnPtBal <= 0.)
    {
        if (pt >= thresholdPtBal)
            return 1.;
        else
            return 0.;
    }
    
    double const x = (pt - thresholdPtBal) / turnOnPtBal;
    
    if (x < 0.)
        return 0.;
    else if (x > 1.)
        return 1.;
    else
        return -2 * std::pow(x, 3) + 3 * std::pow(x, 2);
}
