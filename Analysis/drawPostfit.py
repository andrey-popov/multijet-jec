#!/usr/bin/env python

"""Plots balance observable after the fit.

Data are plotted in ratio to simulation.
"""

import argparse
import math
import os

import ROOT
ROOT.gROOT.SetBatch(True)


def correct_data_balance(histData3D, histSim, corr=lambda x: 1.):
    """Produce data histogram with applied correction."""
    
    # Build an empty histogram with the same binning as in the
    # simulation histogram
    histData = histSim.Clone('histData')
    histData.SetTitle('')
    histData.Reset('mices')
    
    
    # Fill its content computing it from the 3D histogram
    nBins = histData.GetNbinsX()
    
    for wideBin in range(1, nBins + 1):
        
        # Range in ptLeadProj corresponding to the current wide bin
        ptLeadProjMin = histData.GetBinLowEdge(wideBin)
        ptLeadProjMax = histData.GetBinLowEdge(wideBin + 1)
        
        sumMJB = 0.
        sumMJB2 = 0.
        numEvents = 0
        
        for binPtLeadProj in range(1, histData3D.GetNbinsX() + 1):
            
            ptLeadProj = histData3D.GetXaxis().GetBinCenter(binPtLeadProj)
            if ptLeadProj < ptLeadProjMin or ptLeadProj >= ptLeadProjMax:
                continue
            
            for binMJB in range(1, histData3D.GetNbinsY() + 1):
                uncorrMJB = histData3D.GetYaxis().GetBinCenter(binMJB)
                
                for binF in range(1, histData3D.GetNbinsZ() + 1):
                    
                    F = histData3D.GetZaxis().GetBinCenter(binF)
                    mjb = uncorrMJB * corr(ptLeadProj) / corr(F * ptLeadProj / uncorrMJB)
                    n = int(histData3D.GetBinContent(binPtLeadProj, binMJB, binF))
                    
                    numEvents += n
                    sumMJB += n * mjb
                    sumMJB2 += n * mjb**2
        
        
        # Compute mean MJB and its uncertainty
        meanMJB = sumMJB / numEvents
        meanMJBUnc = math.sqrt((sumMJB2 / numEvents - meanMJB**2) / numEvents)
        
        histData.SetBinContent(wideBin, meanMJB)
        histData.SetBinError(wideBin, meanMJBUnc)
    
    return histData



if __name__ == '__main__':
    
    # Set global style
    # ROOT.gStyle.SetHistMinimumZero(True)
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetStripDecimals(False)
    ROOT.TH1.SetDefaultSumw2(True)
    ROOT.TGaxis.SetMaxDigits(3)
    
    ROOT.gStyle.SetTitleFont(42)
    ROOT.gStyle.SetTitleFontSize(0.04)
    ROOT.gStyle.SetTitleFont(42, 'XYZ')
    ROOT.gStyle.SetTitleXOffset(1.0)
    ROOT.gStyle.SetTitleYOffset(1.3)
    ROOT.gStyle.SetTitleSize(0.045, 'XYZ')
    ROOT.gStyle.SetLabelFont(42, 'XYZ')
    ROOT.gStyle.SetLabelOffset(0.007, 'XYZ')
    ROOT.gStyle.SetLabelSize(0.04, 'XYZ')
    ROOT.gStyle.SetNdivisions(508, 'XYZ')
    
    
    # Parse arguments
    argParser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    argParser.add_argument(
        'inputFile',
        help='ROOT file with inputs for the fit'
    )
    argParser.add_argument(
        'formula',
        help='Formula for L3 JEC'
    )
    argParser.add_argument(
        '--min', default=None,
        help='Lower limit for y axis'
    )
    argParser.add_argument(
        '--max', default=None,
        help='Upper limit for y axis'
    )
    argParser.add_argument(
        '-o', '--output', default='fig/postfit',
        help='Name for output file with the plot'
    )
    
    args = argParser.parse_args()
    
    
    # Read data and simulation histograms from the input file
    inputFile = ROOT.TFile(args.inputFile)
    
    histData3D = inputFile.Get('MJB_Pt30/Data')
    histSim = inputFile.Get('MJB_Pt30/Simulation')
    
    for hist in [histData3D, histSim]:
        hist.SetDirectory(None)
    
    inputFile.Close()
    
    
    # Produce histograms for data and all ratios.  To compute ratios,
    # use a copy of the simulation histogram with the errors reset to
    # zero.
    histDataUncorr = correct_data_balance(histData3D, histSim)
    histDataCorr = correct_data_balance(
        histData3D, histSim, corr=lambda x: eval(args.formula)
    )
    
    histSimNoUnc = histSim.Clone('histSimNoUnc')
    for bin in range(0, histSimNoUnc.GetNbinsX() + 2):
        histSimNoUnc.SetBinError(bin, 0.)
    
    histDataUncorr.Divide(histSimNoUnc)
    histDataCorr.Divide(histSimNoUnc)
    histSim.Divide(histSimNoUnc)
    
    
    # Draw the histograms
    canvas = ROOT.TCanvas('canvas', '', 1500, 1000)
    canvas.SetTicks()
    canvas.SetLeftMargin(0.12)
    canvas.SetRightMargin(0.08)
    
    histDataUncorr.SetMarkerStyle(ROOT.kFullCircle)
    histDataUncorr.SetMarkerSize(1.2)
    histDataUncorr.SetMarkerColor(ROOT.kBlack)
    histDataUncorr.SetLineColor(ROOT.kBlack)
    
    histDataCorr.SetMarkerStyle(ROOT.kFullCircle)
    histDataCorr.SetMarkerSize(1.2)
    histDataCorr.SetMarkerColor(ROOT.kAzure - 2)
    histDataCorr.SetLineColor(ROOT.kAzure - 2)
    
    histSim.SetLineColorAlpha(ROOT.kOrange + 2, 0.2)
    histSim.SetFillColorAlpha(ROOT.kOrange + 2, 0.2)
    histSim.SetMarkerColorAlpha(ROOT.kOrange + 2, 0.)
    #^ Could not prevent histogram from drawing markers
    
    histStack = ROOT.THStack(
        'histStack', ';p_{T,lead} #upoint cos #alpha [GeV];R_{MJB}^{Data} / R_{MJB}^{MC}'
    )
    histStack.Add(histSim, 'e2')
    histStack.Add(histDataUncorr, 'p e1 x0')
    histStack.Add(histDataCorr, 'p e1 x0')
    histStack.Draw('nostack')
    
    
    # For some reason minimum of the histogram stack is set at zero.
    # Reset the range over y axis manually.
    minRatio = min(histDataUncorr.GetMinimum(), histDataCorr.GetMinimum(), histSim.GetMinimum())
    maxRatio = max(histDataUncorr.GetMaximum(), histDataCorr.GetMaximum(), histSim.GetMaximum())
    
    if args.min is None:
        histStack.SetMinimum(minRatio - 0.15 * (maxRatio - minRatio))
    else:
        histStack.SetMinimum(float(args.min))
    
    if args.max is None:
        histStack.SetMaximum(maxRatio + 0.15 * (maxRatio - minRatio))
    else:
        histStack.SetMaximum(float(args.max))
    
    
    # Legend and decorations
    legend = ROOT.TLegend(0.15, 0.86 - 0.04 * 3, 0.30, 0.86)
    legend.SetFillColorAlpha(ROOT.kWhite, 0)
    legend.SetTextFont(42)
    legend.SetTextSize(0.035)
    legend.SetBorderSize(0)
    
    legend.AddEntry(histDataUncorr, ' Uncorr. data ', 'p')
    legend.AddEntry(histDataCorr, ' Corr. data ', 'p')
    legend.AddEntry(histSim, ' MC unc. ', 'f')
    
    legend.Draw()
    
    cmsLabel = ROOT.TLatex(0.12, 0.91, '#scale[1.2]{#font[62]{CMS}} #font[52]{Preliminary}')
    cmsLabel.SetNDC()
    cmsLabel.SetTextFont(42)
    cmsLabel.SetTextSize(0.04)
    cmsLabel.SetTextAlign(11)
    cmsLabel.Draw()
    
    energyLabel = ROOT.TLatex(0.92, 0.91, 'Run2016G 4.4 fb^{-1} (13 TeV)')
    energyLabel.SetNDC()
    energyLabel.SetTextFont(42)
    energyLabel.SetTextSize(0.04)
    energyLabel.SetTextAlign(31)
    energyLabel.Draw()
    
    
    # Save the figure
    figDir = os.path.dirname(args.output)
    if figDir and not os.path.exists(figDir):
        os.makedirs(figDir)
    
    for ext in ['pdf', 'root']:
        canvas.Print(args.output + '.' + ext)
