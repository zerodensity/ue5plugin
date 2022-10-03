#pragma once

#include "Engine/EngineCustomTimeStep.h"

#include "IMZClient.h"
#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Engine/TextureRenderTarget2D.h"
#include <queue>
#include <map>

#include "mediaz.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

//#include "DispelUnrealMadnessPrelude.h"
void MemoryBarrier();
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma intrinsic(_InterlockedCompareExchange64)
#define InterlockedCompareExchange64 _InterlockedCompareExchange64
#include <d3d12.h>
#include "AppClient.h"
#include "Windows/HideWindowsPlatformTypes.h"
//#include "DispelUnrealMadnessPostlude.h"

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
	 virtual void Disconnect() override;
	 virtual void NodeRemoved() override;
	 virtual bool IsConnected() override;
	 void InitConnection();
	 bool Tick(float dt);
	 class ClientImpl* Client = 0;

	 
};



