#include <BalanceHists.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>

#include <mensura/extensions/TFileService.hpp>

#include <cmath>


BalanceHists::BalanceHists(std::string const name /*= "BalanceHists"*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    recoilBuilderName("RecoilBuilder"), recoilBuilder(nullptr),
    outDirectoryName(name)
{
    for (double pt = 180.; pt < 2000. + 1.; pt += 5.)
        ptLeadBinning.emplace_back(pt);
    
    for (double pt = 15.; pt < 50. + 0.5; pt += 1.)
        ptJetBinning.emplace_back(pt);
    
    for (double pt = 50.; pt < 200. + 1.; pt += 2.)
        ptJetBinning.emplace_back(pt);
    
    for (double pt = 200.; pt < ptLeadBinning.back() + 1.; pt += 5.)
        ptJetBinning.emplace_back(pt);
}


void BalanceHists::BeginRun(Dataset const &)
{
    // Save pointers to required services and plugins
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    jetmetPlugin = dynamic_cast<JetMETReader const *>(GetDependencyPlugin(jetmetPluginName));
    recoilBuilder = dynamic_cast<RecoilBuilder const *>(GetDependencyPlugin(recoilBuilderName));
    
    
    // Create all histograms
    histPtLead = fileService->Create<TH1D>(outDirectoryName, "PtLead",
      ";p_{T}^{lead} [GeV];Events", ptLeadBinning.size() - 1, ptLeadBinning.data());
    
    profPtLead = fileService->Create<TProfile>(outDirectoryName, "PtLeadProfile",
      ";p_{T}^{lead} [GeV];p_{T}^{lead} [GeV]", ptLeadBinning.size() - 1, ptLeadBinning.data());
    profMJB = fileService->Create<TProfile>(outDirectoryName, "MJBProfile",
      ";p_{T}^{lead} [GeV];MJB", ptLeadBinning.size() - 1, ptLeadBinning.data());
    profMPF = fileService->Create<TProfile>(outDirectoryName, "MPFProfile",
      ";p_{T}^{lead} [GeV];MPF", ptLeadBinning.size() - 1, ptLeadBinning.data());
    
    histJetPtProj = fileService->Create<TH2D>(outDirectoryName, "JetPtProj",
      ";p_{T}^{lead} [GeV];Proj. of jet p_{T} [GeV]",
      ptLeadBinning.size() - 1, ptLeadBinning.data(),
      ptJetBinning.size() - 1, ptJetBinning.data());
    histJetPt = fileService->Create<TH2D>(outDirectoryName, "JetPt",
      ";p_{T}^{lead} [GeV];Jet p_{T} [GeV]",
      ptLeadBinning.size() - 1, ptLeadBinning.data(),
      ptJetBinning.size() - 1, ptJetBinning.data());
}


Plugin *BalanceHists::Clone() const
{
    return new BalanceHists(*this);
}


void BalanceHists::SetDirectoryName(std::string const &name)
{
    outDirectoryName = name;
}


void BalanceHists::SetBinningPtLead(std::vector<double> const &binning)
{
    ptLeadBinning = binning;
}


void BalanceHists::SetBinningPtJetRecoil(std::vector<double> const &binning)
{
    ptJetBinning = binning;
}


void BalanceHists::SetRecoilBuilderName(std::string const &name)
{
    recoilBuilderName = name;
}


bool BalanceHists::ProcessEvent()
{
    auto const &recoil = recoilBuilder->GetP4Recoil();
    auto const &j1 = recoilBuilder->GetP4LeadingJet();
    auto const &recoilJets = recoilBuilder->GetRecoilJets();
    auto const &met = jetmetPlugin->GetMET().P4();
    
    
    histPtLead->Fill(j1.Pt());
    
    double const mjb = recoil.Pt() * std::cos(recoilBuilder->GetAlpha()) / j1.Pt();
    double const mpf = 1. + (met.Px() * j1.Px() + met.Py() * j1.Py()) / std::pow(j1.Pt(), 2);
    profPtLead->Fill(j1.Pt(), j1.Pt());
    profMJB->Fill(j1.Pt(), mjb);
    profMPF->Fill(j1.Pt(), mpf);
    
    for (Jet const &j: recoilJets)
    {
        histJetPtProj->Fill(j1.Pt(), j.Pt(), std::cos(j.Phi() - j1.Phi()));
        histJetPt->Fill(j1.Pt(), j.Pt());
    }
    
    
    return true;
}
