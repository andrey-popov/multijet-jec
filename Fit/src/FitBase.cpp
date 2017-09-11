#include <FitBase.hpp>

#include <cmath>
#include <sstream>
#include <stdexcept>


JetCorrBase::JetCorrBase(unsigned numParams):
    parameters(numParams)
{}


unsigned JetCorrBase::GetNumParams() const
{
    return parameters.size();
}


void JetCorrBase::SetParams(std::vector<double> const &newParams)
{
    if (parameters.size() != newParams.size())
    {
        std::ostringstream message;
        message << "JetCorrBase::SetParams: Number of given parameters (" << newParams.size() <<
          "does not match the expected number (" << parameters.size() << ").";
        throw std::runtime_error(message.str());
    }
    
    parameters = newParams;
}


double JetCorrBase::UndoCorr(double pt, double tolerance) const
{
    // Invert the correction iteratively. Assuming that the correction is a continuously
    //differentiable function of pt, the sought-for uncorrected pt is an attractive stable point
    //of function pt / corr(ptUncorr) if
    //  |pt / c(ptUncorr)|' = pt |c(ptUncorr)|' / c(ptUncorr)^2 < 1
    //(see, for instance, [1]).
    //[1] https://en.wikipedia.org/wiki/Fixed_point_(mathematics)
    
    unsigned const maxIter = 100;
    unsigned iter = 0;
    double ptUncorr = pt / Eval(pt);
    
    while (true)
    {
        double const curCorr = Eval(ptUncorr);
        double const ptRecomp = ptUncorr * curCorr;
        
        if (std::abs(ptRecomp / pt - 1) < tolerance)
            break;
        
        ptUncorr = pt / curCorr;
        
        
        if (iter == maxIter)
        {
            std::ostringstream message;
            message << "Exceeded allowed number of iterations while inverting correction " <<
              "for pt = " << pt << ".";
            throw std::runtime_error(message.str());
        }
    }
    
    return ptUncorr;
}


NuisancesBase const dummyNuisances;
