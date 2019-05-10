#include <EtaPhiFilter.hpp>

#include <mensura/EventIDReader.hpp>
#include <mensura/FileInPath.hpp>
#include <mensura/JetMETReader.hpp>

#include <TFile.h>

#include <cmath>
#include <sstream>
#include <stdexcept>


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


EtaPhiFilter::Region::Region(unsigned long minRun_, unsigned long maxRun_,
  std::string const &filePath, std::string histName):
    minRun(minRun_), maxRun(maxRun_)
{
    TFile srcFile((FileInPath::Resolve("Cleaning", filePath)).c_str());
    
    // If name for the histogram has not been given, assume the file contains only a single object
    if (histName == "")
        histName = srcFile.GetListOfKeys()->First()->GetName();
    
    TH2Poly *hist = dynamic_cast<TH2Poly *>(srcFile.Get(histName.c_str()));
    
    if (not hist)
    {
        std::ostringstream message;
        message << "EtaPhiFilter::Region::Region: Failed to read TH2Poly \"" << histName
          <<"\" from file \"" << filePath << "\".";
        throw std::runtime_error(message.str());
    }
    
    hist->SetDirectory(nullptr);
    map.reset(hist);
    
    srcFile.Close();
}


bool EtaPhiFilter::Region::InEtaPhi(double eta, double phi) const
{
    if (map)
    {
        auto const bin = map->FindBin(eta, phi);
        return (map->GetBinContent(bin) > 0.5);
    }
    else
    {
        if (eta <= minEta or eta >= maxEta)
            return false;
        
        while (phi <= minPhi)
            phi += 2 * M_PI;
        
        return (phi < maxPhi);
    }
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


EtaPhiFilter::EtaPhiFilter(std::string const &name, double minPt, std::string const &filePath,
  std::string const histName):
    EtaPhiFilter(name, minPt)
{
    AddRegion(0, -1, filePath, histName);
}


void EtaPhiFilter::AddRegion(unsigned long minRun, unsigned long maxRun, double startEta,
  double endEta, double startPhi, double endPhi)
{
    regions.emplace_back(minRun, maxRun, startEta, endEta, startPhi, endPhi);
}


void EtaPhiFilter::AddRegion(unsigned long minRun, unsigned long maxRun,
  std::string const &filePath, std::string const histName)
{
    regions.emplace_back(minRun, maxRun, filePath, histName);
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
