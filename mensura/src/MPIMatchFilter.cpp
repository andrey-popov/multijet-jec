#include <MPIMatchFilter.hpp>

#include <mensura/core/GenJetMETReader.hpp>
#include <mensura/core/GenParticleReader.hpp>

#include <TVector2.h>

#include <cmath>
#include <limits>


MPIMatchFilter::MPIMatchFilter(std::string const &name, double maxDR):
    AnalysisPlugin(name),
    genParticlePluginName("GenParticles"), genParticlePlugin(nullptr),
    genJetPluginName("GenJetMET"), genJetPlugin(nullptr),
    maxDR2(std::pow(maxDR, 2))
{}


MPIMatchFilter::MPIMatchFilter(double maxDR):
    MPIMatchFilter("MPIMatchFilter", maxDR)
{}


void MPIMatchFilter::BeginRun(Dataset const &)
{
    genParticlePlugin = \
      dynamic_cast<GenParticleReader const *>(GetDependencyPlugin(genParticlePluginName));
    genJetPlugin = dynamic_cast<GenJetMETReader const *>(GetDependencyPlugin(genJetPluginName));
}


Plugin *MPIMatchFilter::Clone() const
{
    return new MPIMatchFilter(*this);
}


bool MPIMatchFilter::ProcessEvent()
{
    auto const &jets = genJetPlugin->GetJets();
    
    if (jets.size() == 0)
        return false;
    
    
    double bestDR2 = std::numeric_limits<double>::infinity();
    
    for (auto const &p: genParticlePlugin->GetParticles())
    {
        double const dR2 = std::pow(jets[0].Eta() - p.Eta(), 2) +
          std::pow(TVector2::Phi_mpi_pi(jets[0].Phi() - p.Phi()), 2);
        
        if (dR2 < bestDR2)
            bestDR2 = dR2;
    }
    
    
    return (bestDR2 < maxDR2);
}
