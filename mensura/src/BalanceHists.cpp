#include <BalanceHists.hpp>

#include <BalanceCalc.hpp>

#include <mensura/core/JetMETReader.hpp>
#include <mensura/core/Processor.hpp>

#include <mensura/extensions/TFileService.hpp>

#include <cmath>


BalanceHists::BalanceHists(std::string const &name, double minPt_ /*= 15.*/):
    AnalysisPlugin(name),
    fileServiceName("TFileService"), fileService(nullptr),
    jetmetPluginName("JetMET"), jetmetPlugin(nullptr),
    balanceCalcName("BalanceCalc"), balanceCalc(nullptr),
    outDirectoryName(name), minPt(minPt_)
{
    // Construct default binning
    for (int pt = 150; pt < 1000; pt += 5)
        ptLeadBinning.emplace_back(pt);
    
    for (int pt = 1000; pt < 3000; pt += 10)
        ptLeadBinning.emplace_back(pt);
    
    
    for (double pt = minPt, step = 0.25; pt < 40. - step / 2; pt += step)
        ptJetBinning.emplace_back(pt);
    
    for (int pt = 40; pt < 50; pt += 1)
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
    balanceCalc = dynamic_cast<BalanceCalc const *>(GetDependencyPlugin(balanceCalcName));
    
    
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
    histRelPtJetSumProj = fileService->Create<TH2D>(outDirectoryName, "RelPtJetSumProj",
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


bool BalanceHists::ProcessEvent()
{
    auto const &jets = jetmetPlugin->GetJets();
    auto const &j1 = jets.at(0);
    
    
    histPtLead->Fill(j1.Pt());
    profPtLead->Fill(j1.Pt(), j1.Pt());
    profPtBal->Fill(j1.Pt(), balanceCalc->GetPtBal());
    profMPF->Fill(j1.Pt(), balanceCalc->GetMPF());
    
    
    // Remaining histograms are filled with all jets above the threshold but the leading one
    for (unsigned i = 1; i < jets.size(); ++i)
    {
        double const pt = jets[i].Pt();
        
        if (pt < minPt)
            break;
        
        histPtJet->Fill(j1.Pt(), pt);
        histPtJetSumProj->Fill(j1.Pt(), pt, -pt * std::cos(jets[i].Phi() - j1.Phi()));
        histRelPtJetSumProj->Fill(j1.Pt(), pt, -pt * std::cos(jets[i].Phi() - j1.Phi()) / j1.Pt());
    }
    
    
    return true;
}
