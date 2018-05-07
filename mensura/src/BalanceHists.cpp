#include <BalanceHists.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>

#include <mensura/extensions/TFileService.hpp>

#include <cmath>


BalanceHists::BalanceHists(std::string const &name, double minPt_ /*= 15.*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    recoilBuilderName("RecoilBuilder"), recoilBuilder(nullptr),
    outDirectoryName(name), minPt(minPt_)
{
    // Construct default binning
    for (int pt = 150; pt < 1000; pt += 5)
        ptLeadBinning.emplace_back(pt);
    
    for (int pt = 1000; pt < 3000; pt += 10)
        ptLeadBinning.emplace_back(pt);
    
    
    for (int pt = minPt; pt < 50; pt += 1)
        ptJetBinning.emplace_back(pt);
    
    for (int pt = 50; pt < 200; pt += 2)
        ptJetBinning.emplace_back(pt);
    
    for (int pt = 200; pt < 1000; pt += 5)
        ptJetBinning.emplace_back(pt);
    
    for (int pt = 1000; pt <= int(std::round(ptLeadBinning.back())); pt += 10)
        ptJetBinning.emplace_back(pt);
}


BalanceHists::BalanceHists(double minPt /*= 15.*/):
    BalanceHists("BalanceHists", minPt)
{}


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
    profPtBal = fileService->Create<TProfile>(outDirectoryName, "PtBalProfile",
      ";p_{T}^{lead} [GeV];p_{T} balance", ptLeadBinning.size() - 1, ptLeadBinning.data());
    profMPF = fileService->Create<TProfile>(outDirectoryName, "MPFProfile",
      ";p_{T}^{lead} [GeV];MPF", ptLeadBinning.size() - 1, ptLeadBinning.data());
    
    histPtJet = fileService->Create<TH2D>(outDirectoryName, "PtJet",
      ";p_{T}^{lead} [GeV];Jet p_{T} [GeV]",
      ptLeadBinning.size() - 1, ptLeadBinning.data(),
      ptJetBinning.size() - 1, ptJetBinning.data());
    histPtJetSumProj = fileService->Create<TH2D>(outDirectoryName, "PtJetSumProj",
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
    auto const &j1 = recoilBuilder->GetP4LeadingJet();
    auto const &recoil = recoilBuilder->GetP4Recoil();
    auto const &met = jetmetPlugin->GetMET().P4();
    auto const &jets = jetmetPlugin->GetJets();
    
    
    histPtLead->Fill(j1.Pt());
    
    double const ptBal = recoil.Pt() * std::cos(recoilBuilder->GetAlpha()) / j1.Pt();
    double const mpf = 1. + (met.Px() * j1.Px() + met.Py() * j1.Py()) / std::pow(j1.Pt(), 2);
    profPtLead->Fill(j1.Pt(), j1.Pt());
    profPtBal->Fill(j1.Pt(), ptBal);
    profMPF->Fill(j1.Pt(), mpf);
    
    
    // Remaining histograms are filled with all jets above the threshold but the leading one
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        double const pt = jets[i].Pt();
        
        if (pt < minPt)
            break;
        
        histPtJet->Fill(j1.Pt(), pt);
        histPtJetSumProj->Fill(j1.Pt(), pt, pt * std::cos(jets[i].Phi() - j1.Phi()));
    }
    
    
    return true;
}
