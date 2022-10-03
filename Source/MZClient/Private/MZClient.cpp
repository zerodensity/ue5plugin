

#include "MZClient.h"

#include "ScreenRendering.h"
#include "HardwareInfo.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"


#define LOCTEXT_NAMESPACE "FMZClient"

#pragma optimize("", off)

template <class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
static flatbuffers::Offset<mz::app::AppEvent> CreateAppEventOffset(flatbuffers::FlatBufferBuilder& b, flatbuffers::Offset<T> event)
{
	return mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union());
}

class MZCLIENT_API ClientImpl : public mz::app::AppClient
{
public:
    using mz::app::AppClient::AppClient;
    
    virtual void OnAppConnected(mz::app::AppConnectedEvent const& event) override
    {
        FMessageDialog::Debugf(FText::FromString("Connected to mzEngine"), 0);

        if (flatbuffers::IsFieldPresent(&event, mz::app::AppConnectedEvent::VT_NODE))
        {
            nodeId = *(FGuid*)event.node()->id();
        }
    }

    virtual void OnNodeUpdate(mz::NodeUpdated const& archive) override
    {
    }

    virtual void OnMenuFired(mz::ContextMenuRequest const& request) override
    {
    }

    void OnTextureCreated(mz::fb::Texture const& texture)
    {
    }

    virtual void Done(grpc::Status const& Status) override
    {
    }

    virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override
    {
    }

    virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override
    {
    }

    virtual void OnFunctionCall(mz::app::FunctionCall const& action) override
    {
    }

	virtual void OnExecute(mz::app::AppExecute const& aE) override
    {
    }

    FGuid nodeId;
    std::atomic_bool shutdown = true;
};

TMap<FGuid, const mz::fb::Pin*> ParsePins(mz::fb::Node const& archive)
{
	TMap<FGuid, const mz::fb::Pin*> re;
	for (auto pin : *archive.pins())
	{
		re.Add(*(FGuid*)pin->id()->bytes()->Data(), pin);
	}
	return re;
}

FMZClient::FMZClient() {}

bool FMZClient::IsConnected()
{
	return Client && Client->nodeId.IsValid() && !Client->shutdown;
}



void FMZClient::NodeRemoved() 
{
    Client->nodeId = {};
}

void FMZClient::Disconnect() {
    Client->nodeId = {};
}

void FMZClient::InitConnection()
{
    if (Client)
    {
        //if (/*!ctsBound && */ (Client->nodeId.IsValid()))
        //{
        //    //CustomTimeStepImpl = NewObject<UMZCustomTimeStep>();
        //    //auto tis = GEngine->SetCustomTimeStep(CustomTimeStepImpl);
        //    if (tis)
        //    {
        //        ctsBound = true;
        //    }
        //}
        if (Client->shutdown)
        {
            Client->shutdown = (GRPC_CHANNEL_READY != Client->Connect());
        }
        return;
    }
	
    std::string protoPath = (std::filesystem::path(std::getenv("PROGRAMDATA")) / "mediaz" / "core" / "Applications" / "Unreal Engine 5").string();
    Client = new ClientImpl("UE5", "UE5", protoPath.c_str());
}

void FMZClient::StartupModule() {
    using namespace grpc;
    using namespace grpc::internal;
    if (grpc::g_glip == nullptr) {
        static auto* const g_gli = new GrpcLibrary();
        grpc::g_glip = g_gli;
    }
    if (grpc::g_core_codegen_interface == nullptr) {
        static auto* const g_core_codegen = new CoreCodegen();
        grpc::g_core_codegen_interface = g_core_codegen;
    }
    
    //GEngine->SetCustomTimeStep(CustomTimeStepImpl);
    // FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);
    InitConnection();
    FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZClient::Tick));
}

void FMZClient::ShutdownModule() 
{

}



bool FMZClient::Tick(float dt)
{
    InitConnection();
	return true;
}


bool FMZClient::Connect() {
    return true;
}

uint32 FMZClient::Run() {
  return 0;
}
#pragma optimize("", on)
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, MZClient)

//
//#include "DispelUnrealMadnessPostlude.h"

