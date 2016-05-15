#include <DynamicTriggerFilter.hpp>

#include <RecoilBuilder.hpp>

#include <mensura/core/Dataset.hpp>
#include <mensura/core/ROOTLock.hpp>
#include <mensura/PECReader/PECInputData.hpp>

#include <sstream>
#include <stdexcept>


DynamicTriggerFilter::DynamicTriggerFilter(std::string const &name,
  std::initializer_list<std::pair<std::string, double>> triggers_):
    PECTriggerFilter(name),
    recoilBuilderName("RecoilBuilder"), recoilBuilder(nullptr)
{
    for (auto const &t: triggers_)
        triggers.emplace_back(Trigger{t.first, t.second, false});
}


DynamicTriggerFilter::DynamicTriggerFilter(
  std::initializer_list<std::pair<std::string, double>> triggers):
    DynamicTriggerFilter("TriggerFilter", triggers)
{}


void DynamicTriggerFilter::BeginRun(Dataset const &dataset)
{
    // Set up the pointer to PECInputData and register reading of the trigger tree
    PECTriggerFilter::BeginRun(dataset);
    
    // Save pointer to the RecoilBuilder
    recoilBuilder = dynamic_cast<RecoilBuilder const *>(GetDependencyPlugin(recoilBuilderName));
    
    // Memorize the type of the dataset
    isMC = dataset.IsMC();
    
    
    // Make sure that the number of registered triggers matches the number of trigger bins defined
    //by the RecoilBuilder. There is no trigger for the underflow bin
    if (triggers.size() != recoilBuilder->GetNumTriggerBins() - 1)
    {
        std::ostringstream message;
        message << "DynamicTriggerFilter[\"" << GetName() <<
          "\"]::BeginRun: Number of registered triggers (" << triggers.size() <<
          ") does not match number of trigger bins defined by RecoilBuilder \"" <<
          recoilBuilderName << "\" (" << recoilBuilder->GetNumTriggerBins() - 1 <<
          " excluding the underflow bin).";
        throw std::runtime_error(message.str());
    }
    
    
    // Set up relevant branches in the trigger tree
    ROOTLock::Lock();
    triggerTree->SetBranchStatus("*", false);
    
    for (auto &t: triggers)
    {
        // Get the corresponding branch of the tree and make sure it exists
        TBranch *branch = triggerTree->GetBranch((t.name + "__accept").c_str());
        
        if (not branch)
        {
            ROOTLock::Unlock();
            
            std::ostringstream message;
            message << "DynamicTriggerFilter[\"" << GetName() <<
              "\"]::BeginRun: Decision of trigger \"HLT_" << t.name <<
              "_v*\" is not stored in the tree.";
            throw std::runtime_error(message.str());
        }
        
        
        // Set the status and address of the branch
        branch->SetStatus(true);
        branch->SetAddress(&t.buffer);
    }
    
    ROOTLock::Unlock();
}


Plugin *DynamicTriggerFilter::Clone() const
{
    return new DynamicTriggerFilter(*this);
}


double DynamicTriggerFilter::GetWeight() const
{
    if (isMC)
        return triggers.at(requestedTriggerIndex).intLumi;
    else
        return 1.;
}


bool DynamicTriggerFilter::ProcessEvent()
{
    // Read decisions of selected triggers
    inputDataPlugin->ReadEventFromTree(triggerTreeName);
    
    
    // Check decision of the trigger that corresponds to found pt(recoil)
    requestedTriggerIndex = recoilBuilder->GetTriggerBin() - 1;
    return triggers.at(requestedTriggerIndex).buffer;
}