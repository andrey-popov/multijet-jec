#include <DumpEventID.hpp>

#include <mensura/core/EventIDReader.hpp>
#include <mensura/core/Processor.hpp>
#include <mensura/core/ROOTLock.hpp>
#include <mensura/extensions/TFileService.hpp>
#include <mensura/PECReader/PECInputData.hpp>


DumpEventID::DumpEventID(std::string const name /*= "DumpEventID"*/):
    AnalysisPlugin(name),
    eventIDPluginName("InputData"), eventIDPlugin(nullptr),
    fileServiceName("TFileService"), fileService(nullptr),
    treeName(name)
{}


void DumpEventID::BeginRun(Dataset const &)
{
    // Save pointers to required services and plugins
    eventIDPlugin = dynamic_cast<EventIDReader const *>(GetDependencyPlugin(eventIDPluginName));
    fileService = dynamic_cast<TFileService const *>(GetMaster().GetService(fileServiceName));
    
    
    // Create output tree
    tree = fileService->Create<TTree>(directoryName.c_str(), treeName.c_str(),
      "Event ID variables");
    
    ROOTLock::Lock();
    
    tree->Branch("Run", &bfRun);
    tree->Branch("LumiBlock", &bfLumiBlock);
    tree->Branch("Event", &bfEvent);
    tree->Branch("BunchCrossing", &bfBunchCrossing);
    
    ROOTLock::Unlock();
}


Plugin *DumpEventID::Clone() const
{
    return new DumpEventID(*this);
}


void DumpEventID::SetTreeName(std::string const &name)
{
    auto const pos = name.rfind('/');
    
    if (pos != std::string::npos)
    {
        treeName = name.substr(pos + 1);
        directoryName = name.substr(0, pos);
    }
    else
    {
        treeName = name;
        directoryName = "";
    }
}


bool DumpEventID::ProcessEvent()
{
    // Write ID of the current event to the output tree
    auto const &id = eventIDPlugin->GetEventID();
    
    bfRun = id.Run();
    bfLumiBlock = id.LumiBlock();
    bfEvent = id.Event();
    bfBunchCrossing = id.BunchCrossing();
    
    tree->Fill();
    
    
    // This plugin does not perform any event filtering
    return true;
}
