
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


class MZCLIENT_API ClientImpl : public mz::app::AppClient
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

void FMZClient::ShutdownModule() 
{
}

#pragma optimize( "", off )
void FMZClient::SendNodeUpdate(MZEntity entity) 
{
    if (!Client)
    {
        StartupModule();
    }

    if (!Client->id[0])
    {
        return;
    }
    
    //gArena.m_Arena.Reset();

    mz::proto::msg<mz::app::AppEvent> event;
    mz::app::NodeUpdate* req = event->mutable_node_update();
    mz::proto::Pin* pin = req->add_pins_to_add();
    mz::proto::Dynamic* dyn = pin->mutable_dynamic();

    FString id = entity.Entity->GetId().ToString();
    FString label = entity.Entity->GetLabel().ToString();
    
    req->mutable_pins_to_delete()->Clear();

    mz::app::SetFieldByName(req, "node_id", Client->id.c_str());

    
    {
        const std::string& empty = google::protobuf::internal::GetEmptyStringAlreadyInited();
        const std::string& node_id = req->node_id();
        const std::string& pin_id = pin->id();
        const std::string& pin_name = pin->name();
        const std::string& pin_dname = pin->display_name();
    }

    mz::app::SetFieldByName(pin, "id", TCHAR_TO_UTF8(*id));


    {
        const std::string& empty = google::protobuf::internal::GetEmptyStringAlreadyInited();
        const std::string& node_id = req->node_id();
        const std::string& pin_id = pin->id();
        const std::string& pin_name = pin->name();
        const std::string& pin_dname = pin->display_name();
    }

    mz::app::SetFieldByName(pin, "display_name", TCHAR_TO_UTF8(*label));

    {
        const std::string& empty = google::protobuf::internal::GetEmptyStringAlreadyInited();
        const std::string& node_id = req->node_id();
        const std::string& pin_id = pin->id();
        const std::string& pin_name = pin->name();
        const std::string& pin_dname = pin->display_name();
    }

    mz::app::SetFieldByName(pin, "name", TCHAR_TO_UTF8(*label));
    {
        const std::string& empty = google::protobuf::internal::GetEmptyStringAlreadyInited();
        const std::string& node_id = req->node_id();
        const std::string& pin_id = pin->id();
        const std::string& pin_name = pin->name();
        const std::string& pin_dname = pin->display_name();
    }
 
    //*req->mutable_node_id() = Client->id.c_str();
    //*pin->mutable_id() = (TCHAR_TO_UTF8(*id));
    //*pin->mutable_display_name() = (TCHAR_TO_UTF8(*label));
    //*pin->mutable_name() = ( TCHAR_TO_UTF8(*label));

    //{
    //    std::string node_id = req->node_id();
    //    std::string pin_id = pin->id();
    //    std::string pin_name = pin->name();
    //    std::string pin_dname = pin->display_name();
    //}


    // entity.SerializeToProto(dyn);
 
    Client->Write(event);
}

#pragma optimize( "", on )

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

