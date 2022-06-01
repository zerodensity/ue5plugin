
#include "MZClient.h"
#include "HAL/RunnableThread.h"
#include "RemoteControlPreset.h"
#include "IRemoteControlPropertyHandle.h"

#include "Misc/MessageDialog.h"
#include "Runtime/Launch/Resources/Version.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

#include "AppClient.h"

#define LOCTEXT_NAMESPACE "FMZClient"


class ClientImpl : public mz::app::AppClient
{
public:
    using mz::app::AppClient::AppClient;

    virtual void OnAppConnected(mz::app::AppConnectedEvent const& event) override
    {
        FMessageDialog::Debugf(FText::FromString("Connected to mzEngine"), 0);
    }

    virtual void OnNodeUpdate(mz::proto::Node const& archive) override 
    {        
        id = archive.id();
    }

    virtual void OnMenuFired(mz::app::ContextMenuRequest const& request) override
    {
    }

    virtual void Done(grpc::Status Status) override
    {
        FMessageDialog::Debugf(FText::FromString("App Client shutdown"), 0);
        FMZClient::Get()->Disconnect();
    }

    std::string id;
};


FMZClient::FMZClient() {}

void FMZClient::Disconnect() {
    Client = 0;
}

void FMZClient::StartupModule() {

    FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);
    
    std::string protoPath = (std::filesystem::path(std::getenv("PROGRAMDATA")) / "mediaz" / "core" / "UEAppConfig").string();
    Client = new ClientImpl("830121a2-fd7a-4eca-8636-60c895976a71", "Unreal Engine", protoPath.c_str(), true);
}

void FMZClient::ShutdownModule() {
}


void FMZClient::SendNodeUpdate(MZEntity entity) 
{
    if (!Client)
    {
        StartupModule();
    }

    if (Client->id.empty())
    {
        return;
    }

    mz::proto::msg<mz::app::AppEvent> event;
    mz::app::NodeUpdate* req = event->mutable_node_update();
    mz::proto::Pin* pin = req->add_pins_to_add();
    req->mutable_pins_to_delete()->Clear();
    mz::proto::Dynamic* dyn = pin->mutable_dynamic();


    FString id = entity.Entity->GetId().ToString();
    FString label = entity.Entity->GetLabel().ToString();

    req->set_node_id(Client->id);
    req->set_clear(false);
    pin->set_id(TCHAR_TO_UTF8(*id));
    pin->set_display_name(TCHAR_TO_UTF8(*label));
    pin->set_name(TCHAR_TO_UTF8(*label));
    
    entity.SerializeToProto(dyn);
    
    Client->Write(event);
}

bool FMZClient::Connect() {

  uint32_t iWidth = 1;
  uint32_t iHeight = 1;
  IConsoleVariable* MediaZOutputConsoleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MediaZOutput"));
  switch (MediaZOutputConsoleVar->GetInt()) {
    case 1: {
      iWidth = 1280;
      iHeight = 720;
      break;
    }

    case 3: {
      iWidth = 3840;
      iHeight = 2160;
      break;
    }

    case 2:
    default: {
      iWidth = 1920;
      iHeight = 1080;
      break;
    }
  }
  return true;
}

uint32 FMZClient::Run() {
  return 0;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, MZClient)

//
//#include "DispelUnrealMadnessPostlude.h"

