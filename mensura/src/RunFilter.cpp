#include <RunFilter.hpp>

#include <mensura/EventIDReader.hpp>
#include <mensura/Processor.hpp>

#include <sstream>
#include <stdexcept>


RunFilter::RunFilter(std::string const &name, Mode mode_, unsigned long runNumber_):
    AnalysisPlugin(name),
    eventIDPluginName("InputData"), eventIDPlugin(nullptr),
    mode(mode_), runNumber(runNumber_)
{}


RunFilter::RunFilter(Mode mode, unsigned long runNumber):
    RunFilter("RunFilter", mode, runNumber)
{}


RunFilter::RunFilter(RunFilter const &src):
    AnalysisPlugin(src),
    eventIDPluginName(src.eventIDPluginName), eventIDPlugin(nullptr),
    mode(src.mode), runNumber(src.runNumber)
{}


void RunFilter::BeginRun(Dataset const &)
{
    // Save pointer to plugin that produces jets
    eventIDPlugin = dynamic_cast<EventIDReader const *>(GetDependencyPlugin(eventIDPluginName));
}


Plugin *RunFilter::Clone() const
{
    return new RunFilter(*this);
}


void RunFilter::SetEventIDPluginName(std::string const &name)
{
    eventIDPluginName = name;
}


bool RunFilter::ProcessEvent()
{
    auto const &id = eventIDPlugin->GetEventID();
    
    switch (mode)
    {
        case Mode::Less:
            return id.Run() < runNumber;
        
        case Mode::LessEq:
            return id.Run() <= runNumber;
        
        case Mode::Greater:
            return id.Run() > runNumber;
        
        case Mode::GreaterEq:
            return id.Run() >= runNumber;
        
        default:
        {
            std::ostringstream message;
            message << "RunFilter[\"" << GetName() <<
              "\"]::ProcessEvent: Unknown comparison mode.";
            throw std::runtime_error(message.str());
        }
    }
}
