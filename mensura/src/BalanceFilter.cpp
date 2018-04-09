#include <BalanceFilter.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/Processor.hpp>

#include <cmath>


BalanceFilter::BalanceFilter(std::string const &name, double minPtBal_, double maxPtBal_):
    AnalysisPlugin(name),
    minPtBal(minPtBal_), maxPtBal(maxPtBal_), minPtLead(0.),
    recoilBuilderName("RecoilBuilder"), recoilBuilder(nullptr)
{}


BalanceFilter::BalanceFilter(double minPtBal, double maxPtBal):
    BalanceFilter("BalanceFilter", minPtBal, maxPtBal)
{}


void BalanceFilter::BeginRun(Dataset const &)
{
    recoilBuilder = dynamic_cast<RecoilBuilder const *>(GetDependencyPlugin(recoilBuilderName));
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
    auto const &j1 = recoilBuilder->GetP4LeadingJet();
    auto const &recoil = recoilBuilder->GetP4Recoil();
    
    if (j1.Pt() <= minPtLead)
    {
        // Filtering is disabled
        return true;
    }
    
    
    double const ptBal = recoil.Pt() * std::cos(recoilBuilder->GetAlpha()) / j1.Pt();
    
    return (ptBal > minPtBal and ptBal < maxPtBal);
}
