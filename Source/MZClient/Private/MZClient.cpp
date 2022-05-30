
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


struct ClientImpl : mz::app::AppClient
{
    using mz::app::AppClient::AppClient;

    virtual void OnAppConnected(mz::app::AppConnectedEvent const& event) override
    {
        FMessageDialog::Debugf(FText::FromString("Connected to mzEngine"), 0);
    }

    virtual void OnNodeUpdate(mz::proto::Node const& archive) override
    {
    }

    virtual void OnMenuFired(mz::app::ContextMenuRequest const& request) override
    {
    }

    virtual void Done(grpc::Status Status) override
    {
        FMessageDialog::Debugf(FText::FromString("App Client shutdown"), 0);
    }
};


FMZClient::FMZClient() {}


void FMZClient::StartupModule() {

    FModuleManager::Get().LoadModuleChecked("MZProto");

    FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);

    Client = new ClientImpl("830121a2-fd7a-4eca-8636-60c895976a71", "Unreal Engine", "");
}

void FMZClient::ShutdownModule() {
}


void FMZClient::SendNodeUpdate(MZEntity entity) 
{
    mz::proto::LocalArena arena;
    mz::app::AppEvent* event = arena;
    mz::app::NodeUpdateRequest* req = event->mutable_node_update();

    req->set_clear(false);
    req->set_node_id("UNREAL_ENGINE_NODE_ID");

    auto pin = req->add_pins_to_add();
    pin->set_class_name(entity.Type->Name);
    pin->set_display_name(std::string(TCHAR_TO_UTF8(*entity.Entity->GetLabel().ToString())));
    pin->set_name(std::string(TCHAR_TO_UTF8(*entity.Entity->GetLabel().ToString())));

    Client->Write(*event);
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

