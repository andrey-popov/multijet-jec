#!/usr/bin/env python

"""Draws fitted JEC."""

import os
from uuid import uuid4

import ROOT
ROOT.gROOT.SetBatch(True)


class FitFunction:
    """Describes fitted function with uncertainty."""
    
    def __init__(self, formula, parameterNominal, parameterUnc, functionRange, colour=ROOT.kRed):
        
        self.funcNominal = ROOT.TF1(str(uuid4()), formula, functionRange[0], functionRange[1])
        self.funcUp = self.funcNominal.Clone(str(uuid4()))
        self.funcDown = self.funcNominal.Clone(str(uuid4()))
        
        self.funcNominal.SetParameter(0, parameterNominal)
        self.funcUp.SetParameter(0, parameterNominal + parameterUnc)
        self.funcDown.SetParameter(0, parameterNominal - parameterUnc)
        
        self.colour = colour
    
    
    def get_function(self):
        return self.funcNominal
    
    
    def get_uncertainty_band(self, nPoints=50):
        
        graph = ROOT.TGraphAsymmErrors(nPoints)
        
        x = self.funcNominal.GetXmin()
        step = (self.funcNominal.GetXmax() - self.funcNominal.GetXmin()) / (nPoints - 1)
        
        for iPoint in range(nPoints):
            
            nominalValue = self.funcNominal.Eval(x)
            graph.SetPoint(iPoint, x, nominalValue)
            graph.SetPointEYhigh(iPoint, self.funcUp.Eval(x) - nominalValue)
            graph.SetPointEYlow(iPoint, nominalValue - self.funcDown.Eval(x))
            
            x += step
        
        graph.SetFillColorAlpha(self.colour, 0.2)
        self.uncertaintyBand = graph
        return graph
    
    
    def set_colour(self, colour):
        self.colour = colour
        for func in [self.funcNominal, self.funcUp, self.funcDown]:
            func.SetLineColor(colour)



if __name__ == '__main__':
    
    # Fitted functions
    ptRange = (190., 2010.)
    
    funcLinear = FitFunction('1 + [0] * (x - 1000)', 1.00495e-05, 4.45174e-07, ptRange)
    funcLinear.set_colour(ROOT.kOrange + 2)
    
    # funcLinearLoose = FitFunction('1 + [0] * (x - 1000)', 6.32836e-06, 3.13515e-06, ptRange)
    # funcLinearLoose.set_colour(ROOT.kOrange + 2)
    # funcLinearLoose.get_function().SetLineStyle(ROOT.kDashed)
    
    funcLogLinear = FitFunction('1 + [0] * log(x / 1000)', 0.000469321, 1.98598e-05, ptRange)
    funcLogLinear.set_colour(ROOT.kAzure + 4)
    
    # funcLogLinearLoose = FitFunction('1 + [0] * log(x / 1000)', 0.000428643, 0.000184441, ptRange)
    # funcLogLinearLoose.set_colour(ROOT.kAzure + 4)
    # funcLogLinearLoose.get_function().SetLineStyle(ROOT.kDashed)
    
    
    # Global style
    ROOT.gStyle.SetHistMinimumZero(True)
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetStripDecimals(False)
    ROOT.TH1.SetDefaultSumw2(True)
    ROOT.TGaxis.SetMaxDigits(3)
    
    ROOT.gStyle.SetTitleFont(42)
    ROOT.gStyle.SetTitleFontSize(0.04)
    ROOT.gStyle.SetTitleFont(42, 'XYZ')
    ROOT.gStyle.SetTitleXOffset(1.0)
    ROOT.gStyle.SetTitleYOffset(1.1)
    ROOT.gStyle.SetTitleSize(0.045, 'XYZ')
    ROOT.gStyle.SetLabelFont(42, 'XYZ')
    ROOT.gStyle.SetLabelOffset(0.007, 'XYZ')
    ROOT.gStyle.SetLabelSize(0.04, 'XYZ')
    ROOT.gStyle.SetNdivisions(508, 'XYZ')
    
    
    # Draw the functions
    canvas = ROOT.TCanvas('canvas', '', 1500, 1000)
    canvas.SetTicks()
    
    frame = canvas.DrawFrame(ptRange[0], 0.99, ptRange[1], 1.01)
    frame.SetTitle(';p_{T} [GeV];L3Res JEC')
    
    for func in [funcLinear, funcLogLinear]:
        func.get_uncertainty_band().Draw('3 same')
        func.get_function().Draw('l same')
        
    
    # Legend and decorations
    legend = ROOT.TLegend(0.16, 0.86 - 0.04 * 2, 0.29, 0.86)
    legend.SetFillColorAlpha(ROOT.kWhite, 0)
    legend.SetTextFont(42)
    legend.SetTextSize(0.035)
    legend.SetBorderSize(0)
    
    legend.AddEntry(funcLinear.get_function(), ' Linear ', 'l')
    # legend.AddEntry(funcLinearLoose.get_function(), ' Linear w/ 1% unc. ', 'l')
    legend.AddEntry(funcLogLinear.get_function(), ' Log-linear ', 'l')
    # legend.AddEntry(funcLogLinearLoose.get_function(), ' Log-linear w/ 1% unc. ', 'l')
    
    legend.Draw()
    
    cmsLabel = ROOT.TLatex(0.10, 0.91, '#scale[1.2]{#font[62]{CMS}} #font[52]{Preliminary}')
    cmsLabel.SetNDC()
    cmsLabel.SetTextFont(42)
    cmsLabel.SetTextSize(0.04)
    cmsLabel.SetTextAlign(11)
    cmsLabel.Draw()
    
    energyLabel = ROOT.TLatex(0.90, 0.91, 'Run2016G 4.4 fb^{-1} (13 TeV)')
    energyLabel.SetNDC()
    energyLabel.SetTextFont(42)
    energyLabel.SetTextSize(0.04)
    energyLabel.SetTextAlign(31)
    energyLabel.Draw()
    
    
    # Save the figure
    figDir = 'fig/'
    if not os.path.exists(figDir):
        os.makedirs(figDir)
    
    for ext in ['pdf', 'root']:
        canvas.Print(figDir + 'fittedJEC.' + ext)
