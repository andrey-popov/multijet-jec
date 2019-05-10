#include <AngularFilter.hpp>

#include <mensura/JetMETReader.hpp>

#include <TVector2.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>


AngularFilter::AngularFilter(std::string const name):
    AnalysisPlugin(name),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    minDPhi12(0.), maxDPhi12(std::numeric_limits<double>::infinity()),
    minDPhi23(0.), maxDPhi23(std::numeric_limits<double>::infinity()),
    cutDPhi12Set(false), cutDPhi23Set(false)
{}


void AngularFilter::BeginRun(Dataset const &)
{
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
}


AngularFilter *AngularFilter::Clone() const
{
    return new AngularFilter(*this);
}


void AngularFilter::SetDPhi12Cut(double minimum, double maximum)
{
    minDPhi12 = minimum;
    maxDPhi12 = maximum;
    
    if (maximum < minimum)
    {
        std::ostringstream message;
        message << "AngularFilter[\"" << GetName() << "\"]::SetDPhi12Cut: Upper cut (" <<
          maximum << ") is smaller than lower cut (" << minimum << ").";
        throw std::runtime_error(message.str());
    }
    
    // Set a flag showing if this cut can actually reject anything
    cutDPhi12Set = (minDPhi12 > 0. or maxDPhi12 < M_PI);
}


void AngularFilter::SetDPhi23Cut(double minimum, double maximum)
{
    minDPhi23 = minimum;
    maxDPhi23 = maximum;
    
    if (maximum < minimum)
    {
        std::ostringstream message;
        message << "AngularFilter[\"" << GetName() << "\"]::SetDPhi12Cut: Upper cut (" <<
          maximum << ") is smaller than lower cut (" << minimum << ").";
        throw std::runtime_error(message.str());
    }
    
    // Set a flag showing if this cut can actually reject anything
    cutDPhi23Set = (minDPhi23 > 0. or maxDPhi23 < M_PI);
}


bool AngularFilter::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    
    if (cutDPhi12Set)
    {
        if (jets.size() < 2)
            return false;
        
        double const dPhi12 = std::abs(TVector2::Phi_mpi_pi(jets[0].Phi() - jets[1].Phi()));
        
        if (dPhi12 < minDPhi12 or dPhi12 > maxDPhi12)
            return false;
    }
    
    if (cutDPhi23Set)
    {
        if (jets.size() < 3)
            return false;
        
        double const dPhi23 = std::abs(TVector2::Phi_mpi_pi(jets[1].Phi() - jets[2].Phi()));
        
        if (dPhi23 < minDPhi23 or dPhi23 > maxDPhi23)
            return false;
    }
    
    return true;
}
