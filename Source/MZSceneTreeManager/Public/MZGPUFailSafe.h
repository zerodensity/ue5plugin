#pragma once

#include <concrt.h>

class MZGPUFailSafeRunnable final : public FRunnable
{
public:

	MZGPUFailSafeRunnable(struct ID3D12CommandQueue* _CmdQueue, struct ID3D12Device* Device);

	virtual ~MZGPUFailSafeRunnable();

	virtual void Stop() override;

private:
	virtual uint32 Run() override;
	
	bool bExit = false;
	ID3D12CommandQueue* CmdQueue;
	struct ID3D12Fence* Fence;
	uint64_t FenceValue = 0;
	HANDLE Event;
};
