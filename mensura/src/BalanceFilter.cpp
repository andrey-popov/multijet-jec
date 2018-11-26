#include <BalanceFilter.hpp>

#include <BalanceCalc.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>

#include <cmath>


BalanceFilter::BalanceFilter(std::string const &name, double minPtBal_, double maxPtBal_):
    AnalysisPlugin(name),
    minPtBal(minPtBal_), maxPtBal(maxPtBal_), minPtLead(0.),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    balanceCalcName("BalanceCalc"), balanceCalc(nullptr)
{}


BalanceFilter::BalanceFilter(double minPtBal, double maxPtBal):
    BalanceFilter("BalanceFilter", minPtBal, maxPtBal)
{}


void BalanceFilter::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    balanceCalc = dynamic_cast<BalanceCalc const *>(GetDependencyPlugin(balanceCalcName));
}


Plugin *BalanceFilter::Clone() const
{
    return new BalanceFilter(*this);
}


void BalanceFilter::SetMinPtLead(double minPtLead_)
{
    minPtLead = minPtLead_;
}


bool BalanceFilter::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    if (jets.size() < 2)
        return false;
    
    
    if (jets[0].Pt() <= minPtLead)
    {
        // Filtering is disabled
        return true;
    }
    
    
    double const ptBal = balanceCalc->GetPtBal();
    
    return (ptBal > minPtBal and ptBal < maxPtBal);
}
