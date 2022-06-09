#pragma once

#include "IMZClient.h"
#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"

#include <queue>

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

#include "AppClient.h"

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

    virtual void OnTextureCreated(mz::proto::Texture const& texture) override;

    virtual void Done(grpc::Status Status) override
    {
        FMessageDialog::Debugf(FText::FromString("App Client shutdown"), 0);
        IMZClient::Get()->Disconnect();
    }

    std::string id;
};

/**
 * Implements communication with the MediaZ server
 */
class MZCLIENT_API FMZClient : public IMZClient {
 public:
	 FMZClient();

	 virtual void StartupModule() override;
	 virtual void ShutdownModule() override;
	 
	 bool Connect();

	 uint32 Run();
	 virtual void SendNodeUpdate(MZEntity) override;
	 virtual void Disconnect() override;

	 static size_t HashTextureParams(uint32_t width, uint32_t height, uint32_t format, uint32_t usage);
	
	 virtual void QueueTextureCopy(struct ID3D12Resource* res) override;
	 virtual void OnTextureReceived(mz::proto::Texture const& texture) override;

     void InitRHI();
 private:
     struct ID3D12Device* Dev;
     struct ID3D12CommandAllocator* CmdAlloc;
     struct ID3D12CommandQueue* CmdQueue;
     struct ID3D12GraphicsCommandList* CmdList;

	 class ClientImpl* Client;
	 std::unordered_map<size_t, std::queue<ID3D12Resource*>> CopyQueue;
};
