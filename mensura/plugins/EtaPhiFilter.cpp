#include <EtaPhiFilter.hpp>

#include <mensura/core/EventIDReader.hpp>
#include <mensura/core/JetMETReader.hpp>

#include <cmath>


EtaPhiFilter::Region::Region(unsigned long minRun_, unsigned long maxRun_, double minEta_,
  double maxEta_, double minPhi_, double maxPhi_):
    minRun(minRun_), maxRun(maxRun_),
    minEta(minEta_), maxEta(maxEta_),
    minPhi(minPhi_), maxPhi(maxPhi_)
{
    // Convert phi range in a canonical form
    if (minPhi < 0.)
    {
        minPhi += 2 * M_PI;
        maxPhi += 2 * M_PI;
    }
    
    while (maxPhi < minPhi)
        maxPhi += 2 * M_PI;
}


bool EtaPhiFilter::Region::InEtaPhi(double eta, double phi) const
{
    if (eta <= minEta or eta >= maxEta)
        return false;
    
    while (phi <= minPhi)
        phi += 2 * M_PI;
    
    return (phi < maxPhi);
}


bool EtaPhiFilter::Region::InRunRange(unsigned long run) const
{
    return (run >= minRun and run <= maxRun);
}


EtaPhiFilter::EtaPhiFilter(std::string const &name, double minPt_):
    AnalysisPlugin(name),
    eventIDPluginName("InputData"), eventIDPlugin(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    minPt(minPt_)
{}


EtaPhiFilter::EtaPhiFilter(double minPt):
    EtaPhiFilter("EtaPhiFilter", minPt)
{}


void EtaPhiFilter::AddRegion(unsigned long minRun, unsigned long maxRun, double startEta,
  double endEta, double startPhi, double endPhi)
{
    regions.emplace_back(minRun, maxRun, startEta, endEta, startPhi, endPhi);
}


void EtaPhiFilter::BeginRun(Dataset const &)
{
    // Save pointers to other plugins
    eventIDPlugin = dynamic_cast<EventIDReader const *>(GetDependencyPlugin(eventIDPluginName));
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


Plugin *EtaPhiFilter::Clone() const
{
    return new EtaPhiFilter(*this);
}


bool EtaPhiFilter::ProcessEvent()
{
    // Update the list of regions to checked for the current run
    selectedRegions.clear();
    unsigned long const run = eventIDPlugin->GetEventID().Run();
    
    for (auto const &r: regions)
    {
        if (r.InRunRange(run))
            selectedRegions.emplace_back(&r);
    }
    
    if (selectedRegions.empty())
        return true;
    
    
    // Check all jets against all selected regions
    for (auto const &jet: jetmetPlugin->GetJets())
    {
        if (jet.Pt() < minPt)
        {
            // Jets are ordered in pt
            break;
        }
        
        
        for (auto const &r: selectedRegions)
        {
            if (r->InEtaPhi(jet.Eta(), jet.Phi()))
                return false;
        }
    }
    
    
    // The event is accepted if the workflow reaches this point
    return true;
}
