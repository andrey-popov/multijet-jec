#include <Multijet.hpp>

#include <TFile.h>
#include <TKey.h>

#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>


using namespace std::string_literals;


Multijet::Multijet(std::string const &fileName, Multijet::Method method_, double minPt_):
    method(method_), minPt(minPt_)
{
    std::string methodLabel;
    
    if (method == Method::PtBal)
        methodLabel = "PtBal";
    else if (method == Method::MPF)
        methodLabel = "MPF";
    
    
    TFile inputFile(fileName.c_str());
    
    if (inputFile.IsZombie())
    {
        std::ostringstream message;
        message << "Failed to open file \"" << fileName << "\".";
        throw std::runtime_error(message.str());
    }
    
    
    TIter fileIter(inputFile.GetListOfKeys());
    TKey *key;
    
    while ((key = dynamic_cast<TKey *>(fileIter())))
    {
        if (strcmp(key->GetClassName(), "TDirectoryFile") != 0)
            continue;
        
        TDirectoryFile *directory = dynamic_cast<TDirectoryFile *>(key->ReadObj());
        
        for (auto const &name: std::initializer_list<std::string>{"Sim"s + methodLabel + "Profile",
          "PtLead", "PtLeadProfile", methodLabel + "Profile", "PtJet", "PtJetSumProj"})
        {
            if (not directory->Get(name.c_str()))
            {
                std::ostringstream message;
                message << "Multijet::Multijet: Directory \"" << key->GetName() << 
                  "\" in file \"" << fileName << "\" does not contain required key \"" <<
                  name << "\".";
                throw std::runtime_error(message.str());
            }
        }
        
        
        TriggerBin bin;
        
        bin.simBalProfile.reset(dynamic_cast<TProfile *>(
          directory->Get(("Sim"s + methodLabel + "Profile").c_str())));
        bin.balProfile.reset(dynamic_cast<TProfile *>(
          directory->Get((methodLabel + "Profile").c_str())));
        bin.ptLead.reset(dynamic_cast<TH1 *>(directory->Get("PtLead")));
        bin.ptLeadProfile.reset(dynamic_cast<TProfile *>(directory->Get("PtLeadProfile")));
        bin.ptJetSumProj.reset(dynamic_cast<TH2 *>(directory->Get("PtJetSumProj")));
        
        bin.simBalProfile->SetDirectory(nullptr);
        bin.balProfile->SetDirectory(nullptr);
        bin.ptLead->SetDirectory(nullptr);
        bin.ptLeadProfile->SetDirectory(nullptr);
        bin.ptJetSumProj->SetDirectory(nullptr);
        
        triggerBins.emplace_back(std::move(bin));
    }
    
    inputFile.Close();
    
    
    // Construct remaining fields in trigger bins
    for (auto &bin: triggerBins)
    {
        // Save binning in data in a handy format
        bin.binning.resize(bin.ptLead->GetNbinsX() + 1);
        
        for (int i = 1; i <= bin.ptLead->GetNbinsX() + 1; ++i)
            bin.binning.emplace_back(bin.ptLead->GetBinLowEdge(i));
        
        
        // Compute combined (squared) uncertainty on the balance observable in data and simulation.
        //The data profile is rebinned with the binning used for simulation. This is done assuming
        //that bin edges of the two binnings are aligned, which should normally be the case.
        TProfile *balProfileRebinned = dynamic_cast<TProfile *>(bin.balProfile->Rebin(
          bin.simBalProfile->GetNbinsX(), "",
          bin.simBalProfile->GetXaxis()->GetXbins()->GetArray()));
        
        for (int i = 1; i <= bin.simBalProfile->GetNbinsX() + 1; ++i)
        {
            double const unc2 = std::pow(bin.simBalProfile->GetBinError(i), 2) +
              std::pow(balProfileRebinned->GetBinError(i), 2);
            bin.totalUnc2.emplace_back(unc2);
        }
    }
    
    
    // Precompute dimensionality. It is given by binning of simulation.
    dimensionality = 0;
    
    for (auto const &bin: triggerBins)
        dimensionality += bin.simBalProfile->GetNbinsX();
}


unsigned Multijet::GetDim() const
{
    return dimensionality;
}


double Multijet::Eval(JetCorrBase const &corrector, NuisancesBase const &) const
{
    double minPtUncorr = corrector.UndoCorr(minPt);
    
    
    double chi2 = 0.;
    
    for (auto const &triggerBin: triggerBins)
    {
        // The binning in pt of the leading jet in the profile for simulation corresponds to
        //corrected jets. Translate it into a binning in uncorrected pt.
        std::vector<double> uncorrPtBinning;
        
        for (int i = 1; i <= triggerBin.simBalProfile->GetNbinsX() + 1; ++i)
        {
            double const pt = triggerBin.simBalProfile->GetBinLowEdge(i);
            uncorrPtBinning.emplace_back(corrector.UndoCorr(pt));
        }
        
        
        // Build a map from this translated binning to the fine binning in data histograms. It
        //accounts both for the migration in pt of the leading jet due to the jet correction and
        //the typically larger size of bins used for computation of chi2.
        auto binMap = MapBinning(triggerBin.binning, uncorrPtBinning);
        
        // Under- and overflow bins in pt are included in other trigger bins and must be dropped
        binMap.erase(0);
        binMap.erase(triggerBin.simBalProfile->GetNbinsX() + 1);
        
        
        // Find bin in pt of other jets that contains minPtUncorr, and the corresponding inclusion
        //fraction
        auto const *axis = triggerBin.ptJetSumProj->GetYaxis();
        unsigned const minPtBin = axis->FindFixBin(minPtUncorr);
        double const minPtFrac = (minPtUncorr - axis->GetBinLowEdge(minPtBin)) /
          axis->GetBinWidth(minPtBin);
        FracBin const ptJetStart{minPtBin, 1. - minPtFrac};
        
        
        // Compute chi2 with the translated binning
        for (auto const &binMapPair: binMap)
        {
            auto const &binIndex = binMapPair.first;
            auto const &binRange = binMapPair.second;
            
            double meanBal;
            
            if (method == Method::PtBal)
                meanBal = ComputePtBal(triggerBin, binRange[0], binRange[1], ptJetStart, corrector);
            else
                meanBal = ComputeMPF(triggerBin, binRange[0], binRange[1], ptJetStart, corrector);
            
            double const simMeanBal = triggerBin.simBalProfile->GetBinContent(binIndex);
            chi2 += std::pow(meanBal - simMeanBal, 2) / triggerBin.totalUnc2[binIndex - 1];
        }
    }
    
    
    return chi2;
}


double Multijet::ComputeMPF(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
  FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector)
{
    double sumBal = 0., sumWeight = 0.;
    
    // Loop over bins in ptlead
    for (unsigned iPtLead = ptLeadStart.index; iPtLead <= ptLeadEnd.index; ++iPtLead)
    {
        unsigned numEvents = triggerBin.ptLead->GetBinContent(iPtLead);
        
        if (numEvents == 0)
            continue;
        
        double const ptLead = triggerBin.ptLeadProfile->GetBinContent(iPtLead);
        
        
        // Sum over other jets. Consider separately the starting bin, which is only partly
        //included, and the remaining ones
        double sumJets = 0.;
        
        double pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(ptJetStart.index);
        double s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, ptJetStart.index);
        sumJets += (1 - corrector.Eval(pt)) * s * ptJetStart.frac;
        
        for (int iPtJ = ptJetStart.index + 1; iPtJ < triggerBin.ptJetSumProj->GetNbinsY() + 1;
          ++iPtJ)
        {
            pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(iPtJ);
            s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, iPtJ);
            sumJets += (1 - corrector.Eval(pt)) * s;
        }
        
        
        // The first and the last bins are only partially included. Find the inclusion fraction for
        //the current bin. The computation holds true also when the loop runs over only a single
        //bin in ptlead only.
        double fraction = 1.;
        
        if (iPtLead == ptLeadStart.index)
            fraction = ptLeadStart.frac;
        else if (iPtLead == ptLeadEnd.index)
            fraction = ptLeadEnd.frac;
        
        
        sumBal += triggerBin.balProfile->GetBinContent(iPtLead) * numEvents /
          corrector.Eval(ptLead) * fraction;
        sumBal += sumJets / (ptLead * corrector.Eval(ptLead)) * fraction;
        sumWeight += numEvents * fraction;
    }
    
    return sumBal / sumWeight;
}


double Multijet::ComputePtBal(TriggerBin const &triggerBin, FracBin const &ptLeadStart,
  FracBin const &ptLeadEnd, FracBin const &ptJetStart, JetCorrBase const &corrector)
{
    double sumBal = 0., sumWeight = 0.;
    
    // Loop over bins in ptlead
    for (unsigned iPtLead = ptLeadStart.index; iPtLead <= ptLeadEnd.index; ++iPtLead)
    {
        unsigned numEvents = triggerBin.ptLead->GetBinContent(iPtLead);
        
        if (numEvents == 0)
            continue;
        
        double const ptLead = triggerBin.ptLeadProfile->GetBinContent(iPtLead);
        
        
        // Sum over other jets. Consider separately the starting bin, which is only partly
        //included, and the remaining ones
        double sumJets = 0.;
        
        double pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(ptJetStart.index);
        double s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, ptJetStart.index);
        sumJets += s * corrector.Eval(pt) * ptJetStart.frac;
        
        for (int iPtJ = ptJetStart.index + 1; iPtJ < triggerBin.ptJetSumProj->GetNbinsY() + 1;
          ++iPtJ)
        {
            pt = triggerBin.ptJetSumProj->GetYaxis()->GetBinCenter(iPtJ);
            s = triggerBin.ptJetSumProj->GetBinContent(iPtLead, iPtJ);
            sumJets += s * corrector.Eval(pt);
        }
        
        
        // The first and the last bins are only partially included. Find the inclusion fraction for
        //the current bin. The computation holds true also when the loop runs over only a single
        //bin in ptlead only.
        double fraction = 1.;
        
        if (iPtLead == ptLeadStart.index)
            fraction = ptLeadStart.frac;
        else if (iPtLead == ptLeadEnd.index)
            fraction = ptLeadEnd.frac;
        
        
        sumBal += sumJets / (ptLead * corrector.Eval(ptLead)) * fraction;
        sumWeight += numEvents * fraction;
    }
    
    return -sumBal / sumWeight;
}


Multijet::BinMap Multijet::MapBinning(std::vector<double> const &source,
  std::vector<double> const &target)
{
    // Verify that the full range of the target binning is containted within the range of the
    //source binning
    if (target[0] < source[0] or target[target.size() - 1] > source[source.size() - 1])
    {
        std::ostringstream message;
        message << "Multijet::Interpolate: Range of target binning (" << target[0] << ", " <<
          target[target.size() - 1] << ") is not included in the range of source binning (" <<
          source[0] << ", " << source[source.size() - 1] << ").";
        throw std::runtime_error(message.str());
    }
    
    
    // Perform a matching from the target binning to the source one. Each edge of the target
    //binning is represented by the index of the  bin of the source binning that contain this edge
    //and its position within that bin, which is expressed in terms of the bin width. Bins are
    //numbered by the indices of their lower boundaries. The underflow bin has index -1.
    std::vector<FracBin> matchedEdges;
    matchedEdges.reserve(target.size());
    unsigned curSrcBin = -1;
    
    for (auto const &x: target)
    {
        // Scroll to the bin of the source binning that contains value x
        while (curSrcBin < source.size() - 1 and source[curSrcBin + 1] < x)
            ++curSrcBin;
        
        // Find the relative position inside the source bin. The two  special cases can only occur
        //when boundaries of the ranges of the two binnings are approximately equal. The relative
        //positions are set under this assumption.
        double relPos;
        
        if (curSrcBin == unsigned(-1))
            relPos = 1.;
        else if (curSrcBin == source.size() - 1)
            relPos = 0.;
        else
        {
            double const srcBinStart = source[curSrcBin];
            double const srcBinWidth = source[curSrcBin + 1] - srcBinStart;
            relPos = (x - srcBinStart) / srcBinWidth;
        }
        
        matchedEdges.emplace_back(FracBin{curSrcBin, relPos});
    }
    
    
    // Turn the collection of matched edges into a collection of ranges of bins of the source
    //binning. Such a range is built for each target bin.
    std::vector<FracBin> boundaries;
    boundaries.reserve(2 * target.size() + 2);
    unsigned srcBin;
    double fraction;
    
    // The underflow bin for the source binning is always included in the underflow of the target
    boundaries.emplace_back(FracBin{unsigned(-1), 1.});
    
    
    for (unsigned i = 0; i < matchedEdges.size(); ++i)
    {
        srcBin = matchedEdges[i].index;
        double relPos = matchedEdges[i].frac;
        
        // A the moment the algorithm is inside a bin range. Find the closing boundary for this
        //range. If the closing boundary (which is from the source binning) is compatible with the
        //upper edge of the current target bin, treat them as equal. If the relative position is
        //compatible with 0., interpret it as a relative position of 1. within the previous source
        //bin.
        double const tolerance = 1e-7;
        
        if (relPos > 1. - tolerance)
            relPos = 1.;
        
        if (relPos < tolerance and srcBin != unsigned(-1))
        {
            --srcBin;
            relPos = 1.;
        }
        
        
        fraction = relPos;
        
        if (srcBin == boundaries[boundaries.size() - 1].index)
        {
            // If this closing boundary corresponds to the same source bin as the previous
            //(opening) boundary, set its bin fraction to zero in order to simplify iterating over
            //produced bin ranges
            fraction = 0.;
        }
        
        boundaries.emplace_back(FracBin{srcBin, fraction});
        
        
        // Now construct an opening boundary. If the relative position is 1., interpret it as a
        //relative position of 0. within the next source bin in order to avoid bins with an
        //inclusion fraction of zero.
        if (relPos == 1.)
        {
            srcBin += 1;
            relPos = 0.;
        }
        
        if (i < matchedEdges.size() - 1 and matchedEdges[i + 1].index == srcBin)
        {
            // There is more than one target bin edge that is included in the current source bin
            fraction = matchedEdges[i + 1].frac - relPos;
        }
        else
            fraction = 1. - relPos;
        
        boundaries.emplace_back(FracBin{srcBin, fraction});
    }
    
    
    // The last closing boundary is the overflow bin of the source binning. As done for other
    //closing boundaries, set the inclusion fraction to zero when it corresponds to the same source
    //bin as the last opening boundary.
    srcBin = source.size() - 1;
    fraction = (srcBin == boundaries[boundaries.size() - 1].index) ? 0. : 1.;
    boundaries.emplace_back(FracBin{srcBin, fraction});
    
    
    // Convert the collection of constructed boundaries into a bin map. Switch to the bin numbering
    //convention of ROOT, where the underflow bin gets an index of zero.
    BinMap binMap;
    
    for (unsigned targetBin = 0; targetBin < target.size() + 1; ++targetBin)
    {
        auto const &start = boundaries[targetBin * 2];
        auto const &end = boundaries[targetBin * 2 + 1];
        
        std::array<FracBin, 2> range;
        range[0] = {start.index + 1, start.frac};
        range[1] = {end.index + 1, end.frac};
        
        binMap[targetBin] = range;
    }
    
    return binMap;
}
