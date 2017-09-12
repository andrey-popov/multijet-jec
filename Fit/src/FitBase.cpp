#include <FitBase.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>


JetCorrBase::JetCorrBase(unsigned numParams):
    parameters(numParams)
{}


JetCorrBase::~JetCorrBase()
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


void JetCorrBase::SetParams(double const *newParams)
{
    std::copy(newParams, newParams + GetNumParams(), parameters.begin());
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
            message << "JetCorrBase::UndoCorr: Exceeded allowed number of iterations while " <<
              "inverting correction for pt = " << pt << ".";
            throw std::runtime_error(message.str());
        }
    }
    
    return ptUncorr;
}


CombLossFunction::CombLossFunction(std::unique_ptr<JetCorrBase> &&corrector_):
    corrector(std::move(corrector_))
{}


void CombLossFunction::AddMeasurement(DeviationBase const *measurement)
{
    measurements.emplace_back(measurement);
}


unsigned CombLossFunction::GetNDF() const
{
    unsigned dimDeviations = 0;
    
    for (auto const &m: measurements)
        dimDeviations += m->GetDim();
    
    return dimDeviations - GetNumParams();
}


unsigned CombLossFunction::GetNumParams() const
{
    return corrector->GetNumParams();
}


double CombLossFunction::Eval(std::vector<double> const &x) const
{
    if (x.size() != GetNumParams())
    {
        std::ostringstream message;
        message << "CombLossFunction::Eval: Received " << x.size() << " parameters while " <<
          GetNumParams() << " are expected.";
        throw std::runtime_error(message.str());
    }
    
    return EvalRawInput(x.data());
}


double CombLossFunction::Eval(std::vector<double> const &corrParams, Nuisances const &nuisances_)
  const
{
    corrector->SetParams(corrParams);
    SetExternalNuisances(nuisances_);
    
    double loss = 0.;
    
    for (auto const &m: measurements)
        loss += m->Eval(*corrector, nuisances);
    
    return loss;
}


double CombLossFunction::EvalRawInput(double const *x) const
{
    // The input array starts from parameters of the jet correction. It would be followed by
    //values of nuisances to be marginalized, but nuisances are not included in this
    //implementation.
    corrector->SetParams(x);
    
    double loss = 0.;
    
    for (auto const &m: measurements)
        loss += m->Eval(*corrector, nuisances);
    
    return loss;
}


void CombLossFunction::SetExternalNuisances(Nuisances const &nuisances_) const
{
    nuisances = nuisances_;
}
