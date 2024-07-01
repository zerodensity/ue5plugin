/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include <concrt.h>

class NOSGPUFailSafeRunnable final : public FRunnable
{
public:

	NOSGPUFailSafeRunnable(struct ID3D12CommandQueue* _CmdQueue, struct ID3D12Device* Device);

	virtual ~NOSGPUFailSafeRunnable();

	virtual void Stop() override;

private:
	virtual uint32 Run() override;
	
	bool bExit = false;
	ID3D12CommandQueue* CmdQueue;
	struct ID3D12Fence* Fence;
	uint64_t FenceValue = 0;
	HANDLE Event;
};
