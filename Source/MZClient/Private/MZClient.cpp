
#include "MZClient.h"
#include "HAL/RunnableThread.h"
#include "RemoteControlPreset.h"
#include "IRemoteControlPropertyHandle.h"

#include "Runtime/Launch/Resources/Version.h"


#define LOCTEXT_NAMESPACE "FMZClient"



FMZClient::FMZClient() {}

class MZCLIENT_API ClientImpl : public mz::app::AppClient
{
public:
    using mz::app::AppClient::AppClient;

    virtual void OnAppConnected(mz::app::AppConnectedEvent const& event) override
    {
        FMessageDialog::Debugf(FText::FromString("Connected to mzEngine"), 0);
    }

    virtual void OnNodeUpdate(mz::proto::Node const& archive) override;
    virtual void OnMenuFired(mz::app::ContextMenuRequest const& request) override
    {
    }

    void OnTextureCreated(mz::proto::Texture const& texture)
    {
        //IMZClient::Get()->OnTextureReceived(texture);
    }

    virtual void Done(grpc::Status const& Status) override
    {
        FMessageDialog::Debugf(FText::FromString("App Client shutdown"), 0);
        IMZClient::Get()->Disconnect();
    }

    std::string id;
};


size_t FMZClient::HashTextureParams(uint32_t width, uint32_t height, uint32_t format, uint32_t usage)
{
    return
        (0xd24908d710a ^ ((size_t)width << 40ull)) |
        (0X6a6826a9abd ^ ((size_t)height << 18ull)) |
        ((size_t)usage << (6ull)) | (size_t)(format);
}

void FMZClient::Disconnect() {
    Client = 0;
    PendingCopyQueue.Empty();
    CopyOnTick.Empty();
}

void FMZClient::StartupModule() {

    // FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);
    std::string protoPath = (std::filesystem::path(std::getenv("PROGRAMDATA")) / "mediaz" / "core" / "UEAppConfig").string();
    Client = new ClientImpl("830121a2-fd7a-4eca-8636-60c895976a71", "Unreal Engine", protoPath.c_str(), true);
    InitRHI();
    
    FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZClient::Tick));
}

void FMZClient::ShutdownModule() 
{
}

#pragma optimize( "", off )

void FMZClient::SendPinValueChanged(MZEntity entity)
{
    SendNodeUpdate(entity);
}

void FMZClient::SendNodeUpdate(MZEntity entity) 
{
    if (!Client)
    {
        std::string protoPath = (std::filesystem::path(std::getenv("PROGRAMDATA")) / "mediaz" / "core" / "UEAppConfig").string();
        Client = new ClientImpl("830121a2-fd7a-4eca-8636-60c895976a71", "Unreal Engine", protoPath.c_str(), true);
    }

    if (!Client->id[0])
    {
        return;
    }

    mz::proto::msg<mz::app::AppEvent> event;
    mz::app::NodeUpdate* req = event->mutable_node_update();
    mz::proto::Pin* pin = req->add_pins_to_add();
    mz::proto::Dynamic* dyn = pin->mutable_dynamic();
 
    FString id = entity.Entity->GetId().ToString();
    FString label = entity.Entity->GetLabel().ToString();
    
    pin->set_pin_show_as(mz::proto::ShowAs::OUTPUT_PIN);
    pin->set_pin_can_show_as(mz::proto::CanShowAs::OUTPUT_PIN_ONLY);

    mz::app::SetField(req, mz::app::NodeUpdate::kNodeIdFieldNumber, Client->id.c_str());
    mz::app::SetField(pin, mz::proto::Pin::kIdFieldNumber, TCHAR_TO_UTF8(*id));
    mz::app::SetField(pin, mz::proto::Pin::kDisplayNameFieldNumber, TCHAR_TO_UTF8(*label));
    mz::app::SetField(pin, mz::proto::Pin::kNameFieldNumber, TCHAR_TO_UTF8(*label));
    entity.SerializeToProto(dyn);

    Client->Write(event);
}

#pragma optimize( "", on )

bool FMZClient::Connect() {
    return true;
}

uint32 FMZClient::Run() {
  return 0;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, MZClient)

//
//#include "DispelUnrealMadnessPostlude.h"

