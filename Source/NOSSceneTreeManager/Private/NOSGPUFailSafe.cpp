#include "MZGPUFailSafe.h"
#include "MZTextureShareManager.h"
#include "MZSceneTreeManager.h"

MZGPUFailSafeRunnable::MZGPUFailSafeRunnable(ID3D12CommandQueue* _CmdQueue, ID3D12Device* Device) : CmdQueue(_CmdQueue)
{
	Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
	Event = CreateEventA(0, 0, 0, 0);
}

MZGPUFailSafeRunnable::~MZGPUFailSafeRunnable()
{
	bExit = true;
}

void MZGPUFailSafeRunnable::Stop()
{
	bExit = true;
}

uint32 MZGPUFailSafeRunnable::Run()
{
	while (!bExit)
	{
		auto Start = FPlatformTime::Seconds();
		CmdQueue->Signal(Fence, ++FenceValue);
		
		if (Fence->GetCompletedValue() < FenceValue)
		{
			Fence->SetEventOnCompletion(FenceValue, Event);
			if(WaitForSingleObject(Event, 2000) == WAIT_TIMEOUT)
			{
				UE_LOG(LogTemp, Error, TEXT("GPU 2 sec timeout, trying to recover shortly..."));
				auto MZSceneTreeManager = &FModuleManager::LoadModuleChecked<FMZSceneTreeManager>("MZSceneTreeManager");
				MZSceneTreeManager->ExecutionState = mz::app::ExecutionState::IDLE; 
				auto TextureManager = MZTextureShareManager::GetInstance();
				if(TextureManager->InputFence && TextureManager->OutputFence)
				{
					TextureManager->ExecutionState = mz::app::ExecutionState::IDLE;
					for(int i = 0; i < 5; i++)
					{
						TextureManager->InputFence->Signal(UINT64_MAX);
						TextureManager->OutputFence->Signal(UINT64_MAX);
						FPlatformProcess::Sleep(0.2);
					}
				}
				if(MZSceneTreeManager->MZClient)
				{
					flatbuffers::FlatBufferBuilder mb;
					auto offset = mz::CreateAppEventOffset(mb ,mz::app::CreateRecoverSync(mb, (mz::fb::UUID*)&FMZClient::NodeId));
					mb.Finish(offset);
					auto buf = mb.Release();
					auto root = flatbuffers::GetRoot<mz::app::AppEvent>(buf.data());
					MZSceneTreeManager->MZClient->AppServiceClient->Send(*root);
				}
			}
			else
			{
				auto WaitStart = FPlatformTime::Seconds();
				FPlatformProcess::Sleep(2.0 - (WaitStart - Start));
			}
		}
	}
	return 0;
}
