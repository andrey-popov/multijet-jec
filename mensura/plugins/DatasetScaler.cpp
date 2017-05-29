#include <DatasetScaler.hpp>

#include <mensura/core/Dataset.hpp>

#include <sstream>
#include <stdexcept>


DatasetScaler::DatasetScaler(std::string const &name, double targetLumi_, unsigned seed /*= 0*/):
    AnalysisPlugin(name),
    rGen(seed),
    targetLumi(targetLumi_), acceptFraction(0.)
{}


DatasetScaler::DatasetScaler(double targetLumi, unsigned seed /*= 0*/):
    DatasetScaler("DatasetScaler", targetLumi, seed)
{}


void DatasetScaler::BeginRun(Dataset const &dataset)
{
    auto const &firstFile = dataset.GetFiles().front();
    double const effLumi = firstFile.nEvents / firstFile.xSec;
    
    if (effLumi < targetLumi)
    {
        std::ostringstream message;
        message << "Effective luminosity for dataset \"" << dataset.GetSourceDatasetID() <<
          "\" (" << effLumi << "/pb) is smaller than the requested target luminosity (" <<
          targetLumi << "/pb).";
        throw std::runtime_error(message.str());
    }
    
    acceptFraction = targetLumi / effLumi;
}


Plugin *DatasetScaler::Clone() const
{
    return new DatasetScaler(*this);
}


bool DatasetScaler::ProcessEvent()
{
    return (rGen.Rndm() > acceptFraction);
}
