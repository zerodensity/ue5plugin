#pragma once

#include "Engine/EngineCustomTimeStep.h"

#include "IMZClient.h"
#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "RemoteControlPreset.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"

#include <queue>
#include <map>

#include "mediaz.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

#include "DispelUnrealMadnessPrelude.h"
#include <d3d12.h>
#include "AppClient.h"
#include "DispelUnrealMadnessPostlude.h"

#include "D3D12RHIPrivate.h"
#include "D3D12RHI.h"
#include "D3D12Resources.h"

#include "MZCustomTimeStep.h"

#include <mzFlatBuffersCommon.h>


using MessageBuilder = flatbuffers::grpc::MessageBuilder;

template<class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
static flatbuffers::grpc::Message<mz::app::AppEvent> MakeAppEvent(MessageBuilder& b, flatbuffers::Offset<T> event)
{
	b.Finish(mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union()));
	auto msg = b.ReleaseMessage<mz::app::AppEvent>();
	assert(msg.Verify());
	return msg;
}


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

	 virtual void OnNodeUpdateReceived(mz::fb::Node const&) override;

	 virtual void SendNodeUpdate(TMap<FGuid, MZRemoteValue*> const& entities, TMap<FGuid, MZFunction*> const& functions) override;
	 virtual void SendPinRemoved(FGuid) override;
	 virtual void SendPinAdded(MZRemoteValue*) override;
	 virtual void SendFunctionAdded(MZFunction* mzFunc) override;
	 virtual void SendFunctionRemoved(FGuid guid) override;
	 virtual void SendPinValueChanged(MZRemoteValue*) override;
	 

	 virtual void Disconnect() override;
	 virtual void NodeRemoved() override;

	 virtual void FreezeTextures(TArray<FGuid>) override;

	 void ClearResources();

	 virtual void QueueTextureCopy(FGuid id, MZRemoteValue* mzrv, mz::fb::Texture* tex) override;
	 virtual void OnTextureReceived(FGuid id, mz::fb::Texture const& texture) override;
	 virtual void OnPinShowAsChanged(FGuid, mz::fb::ShowAs) override;
	 virtual void OnPinValueChanged(FGuid, const void*, size_t) override;
	 virtual void OnFunctionCall(FGuid nodeId, FGuid funcId) override;

	 void WaitCommands();
	 void ExecCommands();

     void InitRHI();
     
	 void InitConnection();

	 bool Tick(float dt);


	 std::atomic_bool bClientShouldDisconnect = false;

	 struct ResourceInfo
	 {
		 MZRemoteValue* SrcMzrc = 0;
		 ID3D12Resource* DstResource = 0;
		 bool ReadOnly = true;
		 MzTextureShareInfo Info = {};
		 void Release()
		 {
			 if(DstResource) DstResource->Release();
			 memset(this, 0, sizeof(*this));
		 }
	 };

	 struct ID3D12Device* Dev;
	 struct ID3D12CommandAllocator* CmdAlloc;
	 struct ID3D12CommandQueue* CmdQueue;
	 struct ID3D12GraphicsCommandList* CmdList;
	 struct ID3D12Fence* CmdFence;
	 HANDLE CmdEvent;
	 uint64_t CmdFenceValue = 0;

	 class ClientImpl* Client = 0;

	 std::mutex PendingCopyQueueMutex;
	 TMap<FGuid, MZRemoteValue*> PendingCopyQueue;

	 std::mutex CopyOnTickMutex;
	 TMap<FGuid, ResourceInfo> CopyOnTick;

	 std::mutex ResourceChangedMutex;
	 TMap<FGuid, MZRemoteValue*> ResourceChanged;
	 
	 std::mutex ValueUpdatesMutex;
	 TMap<FGuid, std::vector<uint8>> ValueUpdates;

	 std::map< std::string, std::pair<UObject*, FRemoteControlFunction>> functionMap;

	 std::mutex FunctionsMutex;
	 std::queue<FRemoteControlFunction> Functions;

	 UMZCustomTimeStep* CustomTimeStepImpl;
};


