/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Copyright (c) Bingxing Wang. All Rights Reserved.
	Copyright (c) LumiaWoA authors. All Rights Reserved.

	Module Name:

		power.c

	Abstract:

		Contains FingerTipS power-on and power-off functionality

	Environment:

		Kernel mode

	Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <controller.h>
#include <device.h>
#include <spb.h>
#include <fts521\fts521internal.h>
#include <fts521\fts521regs.h>
#include <internal.h>
#include <touch_power\touch_power.h>
#include <power.tmh>

typedef struct _DISPLAY_STATE_WORKITEM_CONTEXT
{
	WDFDEVICE FxDevice;
} DISPLAY_STATE_WORKITEM_CONTEXT, *PDISPLAY_STATE_WORKITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DISPLAY_STATE_WORKITEM_CONTEXT, GetDisplayStateWorkItemContext)

EVT_WDF_WORKITEM TchDisplayStateWorkItem;

static
NTSTATUS
TchAcquireControllerLock(
	_In_ FTS521_CONTROLLER_CONTEXT* ControllerContext
	)
{
	LONGLONG lockTimeout;
	NTSTATUS status;

	if (ControllerContext == NULL ||
		ControllerContext->ControllerLock == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"Controller lock is not initialized - 0x%08lX",
			status);
		return status;
	}

	lockTimeout = TOUCH_REL_TIMEOUT_MS(TOUCH_CONTROLLER_LOCK_TIMEOUT_MS);
	status = WdfWaitLockAcquire(ControllerContext->ControllerLock, &lockTimeout);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"Timeout acquiring controller lock - 0x%08lX",
			status);
	}

	return status;
}

static
PCSTR
TchRuntimeStateName(
	IN TOUCH_RUNTIME_STATE State
)
{
	switch (State)
	{
	case TouchRuntimeUninitialized:
		return "UNINITIALIZED";
	case TouchRuntimePrepared:
		return "PREPARED";
	case TouchRuntimeStarted:
		return "STARTED";
	case TouchRuntimeD0Active:
		return "D0_ACTIVE";
	case TouchRuntimeDisplayOff:
		return "DISPLAY_OFF";
	case TouchRuntimeResetting:
		return "RESETTING";
	case TouchRuntimeD3:
		return "D3";
	case TouchRuntimeFailed:
		return "FAILED";
	default:
		return "UNKNOWN";
	}
}

static
PCSTR
TchResetStateName(
	IN TOUCH_RESET_STATE State
)
{
	switch (State)
	{
	case TouchResetIdle:
		return "RESET_IDLE";
	case TouchResetAssertLow:
		return "RESET_ASSERT_LOW";
	case TouchResetReleaseHigh:
		return "RESET_RELEASE_HIGH";
	case TouchResetWaitControllerReady:
		return "RESET_WAIT_CONTROLLER_READY";
	case TouchResetComplete:
		return "RESET_COMPLETE";
	case TouchResetFailed:
		return "RESET_FAILED";
	default:
		return "UNKNOWN";
	}
}

static
PCSTR
TchInterruptStateName(
	IN TOUCH_INTERRUPT_STATE State
)
{
	switch (State)
	{
	case TouchInterruptNotCreated:
		return "NOT_CREATED";
	case TouchInterruptCreated:
		return "CREATED";
	case TouchInterruptWdfEnabled:
		return "WDF_ENABLED";
	case TouchInterruptWdfDisabled:
		return "WDF_DISABLED";
	default:
		return "UNKNOWN";
	}
}

static
VOID
TchSetRuntimeState(
	IN PDEVICE_EXTENSION DevContext,
	IN TOUCH_RUNTIME_STATE NewState,
	IN PCSTR Source
)
{
	TOUCH_RUNTIME_STATE oldState;

	oldState = DevContext->RuntimeState;
	DevContext->RuntimeState = NewState;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s runtime state %s -> %s",
		Source,
		TchRuntimeStateName(oldState),
		TchRuntimeStateName(NewState));
}

static
VOID
TchSetResetState(
	IN PDEVICE_EXTENSION DevContext,
	IN TOUCH_RESET_STATE NewState,
	IN PCSTR Source
)
{
	TOUCH_RESET_STATE oldState;

	oldState = DevContext->ResetState;
	DevContext->ResetState = NewState;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s reset state %s -> %s",
		Source,
		TchResetStateName(oldState),
		TchResetStateName(NewState));
}

static
ULONG
TchDisplayStateForD0Restore(
	IN ULONG DisplayState
)
{
	if (DisplayState == 0)
	{
		return 0;
	}

	return 1;
}

static
NTSTATUS
TchAppendDisplayStateEventLocked(
	IN PDEVICE_EXTENSION DevContext,
	IN ULONG DisplayState,
	IN ULONG PreviousDisplayState,
	IN ULONG Sequence,
	OUT BOOLEAN* EnqueueWorkItem,
	IN PCSTR Source
)
{
	ULONG queueIndex;
	NTSTATUS status;

	*EnqueueWorkItem = FALSE;

	if (DevContext->DisplayStateQueueCount >= TOUCH_DISPLAY_STATE_QUEUE_DEPTH)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s display state queue overflow state=%lu previous=%lu sequence=%lu depth=%lu runtime=%s - 0x%08lX",
			Source,
			DisplayState,
			PreviousDisplayState,
			Sequence,
			(ULONG)TOUCH_DISPLAY_STATE_QUEUE_DEPTH,
			TchRuntimeStateName(DevContext->RuntimeState),
			status);
		return status;
	}

	queueIndex = (DevContext->DisplayStateQueueHead + DevContext->DisplayStateQueueCount) %
		TOUCH_DISPLAY_STATE_QUEUE_DEPTH;
	DevContext->DisplayStateQueue[queueIndex].DisplayState = DisplayState;
	DevContext->DisplayStateQueue[queueIndex].PreviousDisplayState = PreviousDisplayState;
	DevContext->DisplayStateQueue[queueIndex].Sequence = Sequence;
	DevContext->DisplayStateQueueCount++;

	if (DevContext->DisplayStateWorkQueued == FALSE)
	{
		DevContext->DisplayStateWorkQueued = TRUE;
		*EnqueueWorkItem = TRUE;
		WdfWorkItemEnqueue(DevContext->DisplayStateWorkItem);
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
TchPrepareDisplayStateRestore(
	IN PDEVICE_EXTENSION DevContext,
	OUT ULONG* DisplayState,
	OUT ULONG* Sequence,
	IN PCSTR Source
)
{
	NTSTATUS status;

	if (DevContext->DisplayStateLock == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot prepare display restore because display lock is not initialized - 0x%08lX",
			Source,
			status);
		return status;
	}

	WdfWaitLockAcquire(DevContext->DisplayStateLock, NULL);

	if (DevContext->RuntimeState == TouchRuntimeFailed)
	{
		status = STATUS_DEVICE_NOT_READY;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot enter D0 display restore because runtime is FAILED - 0x%08lX",
			Source,
			status);
		WdfWaitLockRelease(DevContext->DisplayStateLock);
		return status;
	}

	if (DevContext->DisplayStateQueueCount != 0 ||
		DevContext->DisplayStateWorkQueued != FALSE)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot enter D0 display restore with queued display work count=%lu workQueued=%d runtime=%s - 0x%08lX",
			Source,
			DevContext->DisplayStateQueueCount,
			DevContext->DisplayStateWorkQueued,
			TchRuntimeStateName(DevContext->RuntimeState),
			status);
		WdfWaitLockRelease(DevContext->DisplayStateLock);
		return status;
	}

	DevContext->DisplayStateHardwareReady = TRUE;
	DevContext->DisplayStateAcceptingWork = FALSE;
	*DisplayState = DevContext->LastDisplayState;
	*Sequence = DevContext->DisplayStateSequence;
	status = STATUS_SUCCESS;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s prepared display restore state=%lu sequence=%lu runtime=%s",
		Source,
		*DisplayState,
		*Sequence,
		TchRuntimeStateName(DevContext->RuntimeState));

	WdfWaitLockRelease(DevContext->DisplayStateLock);
	return status;
}

static
NTSTATUS
TchFinishDisplayStateRestore(
	IN PDEVICE_EXTENSION DevContext,
	IN ULONG SnapshotDisplayState,
	IN ULONG AppliedDisplayState,
	IN ULONG SnapshotSequence,
	IN PCSTR Source
)
{
	BOOLEAN enqueueWorkItem;
	ULONG currentDisplayState;
	ULONG currentSequence;
	NTSTATUS status;

	enqueueWorkItem = FALSE;
	currentDisplayState = TOUCH_DISPLAY_STATE_UNKNOWN;
	currentSequence = 0;
	status = STATUS_SUCCESS;

	WdfWaitLockAcquire(DevContext->DisplayStateLock, NULL);

	if (DevContext->DisplayStateHardwareReady == FALSE ||
		DevContext->DisplayStateAcceptingWork != FALSE ||
		DevContext->DisplayStateQueueCount != 0 ||
		DevContext->DisplayStateWorkQueued != FALSE)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot finish display restore ready=%d accepting=%d count=%lu workQueued=%d runtime=%s - 0x%08lX",
			Source,
			DevContext->DisplayStateHardwareReady,
			DevContext->DisplayStateAcceptingWork,
			DevContext->DisplayStateQueueCount,
			DevContext->DisplayStateWorkQueued,
			TchRuntimeStateName(DevContext->RuntimeState),
			status);
		WdfWaitLockRelease(DevContext->DisplayStateLock);
		return status;
	}

	currentDisplayState = DevContext->LastDisplayState;
	currentSequence = DevContext->DisplayStateSequence;

	if (currentSequence != SnapshotSequence &&
		currentDisplayState != SnapshotDisplayState &&
		currentDisplayState != TOUCH_DISPLAY_STATE_UNKNOWN)
	{
		status = TchAppendDisplayStateEventLocked(
			DevContext,
			currentDisplayState,
			AppliedDisplayState,
			currentSequence,
			&enqueueWorkItem,
			Source);
	}

	if (NT_SUCCESS(status))
	{
		DevContext->DisplayStateAcceptingWork = TRUE;
	}

	WdfWaitLockRelease(DevContext->DisplayStateLock);

	if (!NT_SUCCESS(status))
	{
		TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
		return status;
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s finished display restore snapshot=%lu applied=%lu current=%lu snapshotSequence=%lu currentSequence=%lu queued=%d",
		Source,
		SnapshotDisplayState,
		AppliedDisplayState,
		currentDisplayState,
		SnapshotSequence,
		currentSequence,
		enqueueWorkItem);

	if (enqueueWorkItem)
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_POWER,
			"%s display restore enqueued deferred display work",
			Source);
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
TchRequireDisplayStateHardwareReady(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
)
{
	BOOLEAN hardwareReady;
	NTSTATUS status;

	if (DevContext->DisplayStateLock == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot access display hardware because display lock is not initialized - 0x%08lX",
			Source,
			status);
		return status;
	}

	WdfWaitLockAcquire(DevContext->DisplayStateLock, NULL);
	hardwareReady = DevContext->DisplayStateHardwareReady;
	WdfWaitLockRelease(DevContext->DisplayStateLock);

	if (hardwareReady == FALSE)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s rejected display hardware access outside D0 runtime=%s - 0x%08lX",
			Source,
			TchRuntimeStateName(DevContext->RuntimeState),
			status);
		TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
		return status;
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
TchWaitForControllerReady(
	IN SPB_CONTEXT* SpbContext
)
{
	BYTE readCommand[1] = { FIFO_CMD_READONE };
	BYTE eventData[FIFO_EVENT_SIZE];
	LARGE_INTEGER delay;
	NTSTATUS status;
	ULONG elapsedMs;

	elapsedMs = 0;
	status = STATUS_TIMEOUT;

	while (elapsedMs < TOUCH_CONTROLLER_READY_TIMEOUT_MS)
	{
		RtlZeroMemory(eventData, sizeof(eventData));

		status = FtsWriteReadU8UX(
			SpbContext,
			readCommand,
			sizeof(readCommand),
			eventData,
			sizeof(eventData));

		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_POWER,
				"TchWaitForControllerReady read failed - 0x%08lX",
				status);
			return status;
		}

		if (eventData[0] == EVT_ID_CONTROLLER_READY)
		{
			Trace(
				TRACE_LEVEL_INFORMATION,
				TRACE_POWER,
				"TchWaitForControllerReady found controller ready after %lu ms",
				elapsedMs);
			return STATUS_SUCCESS;
		}

		if (eventData[0] == EVT_ID_ERROR)
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_POWER,
				"TchWaitForControllerReady observed controller error event type=0x%02X",
				eventData[1]);
			return STATUS_IO_DEVICE_ERROR;
		}

		if (eventData[0] != EVT_ID_NOEVENT)
		{
			Trace(
				TRACE_LEVEL_INFORMATION,
				TRACE_POWER,
				"TchWaitForControllerReady observed event 0x%02X while waiting",
				eventData[0]);
		}

		delay.QuadPart = TOUCH_REL_TIMEOUT_MS(TOUCH_CONTROLLER_READY_POLL_INTERVAL_MS);
		KeDelayExecutionThread(KernelMode, FALSE, &delay);
		elapsedMs += TOUCH_CONTROLLER_READY_POLL_INTERVAL_MS;
	}

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_POWER,
		"TchWaitForControllerReady timed out after %lu ms",
		elapsedMs);

	return STATUS_TIMEOUT;
}

static
NTSTATUS
TchFlushControllerFifo(
	IN SPB_CONTEXT* SpbContext
)
{
	NTSTATUS status;

	status = Fts521FlushFifo(SpbContext);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"TchFlushControllerFifo failed flushing controller FIFO - 0x%08lX",
			status);
	}

	return status;
}

static
VOID
TchDelayMilliseconds(
	IN ULONG Milliseconds
)
{
	LARGE_INTEGER delay;

	delay.QuadPart = TOUCH_REL_TIMEOUT_MS(Milliseconds);
	KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

static
NTSTATUS
TchResetControllerLocked(
	IN PDEVICE_EXTENSION DevContext,
	IN SPB_CONTEXT* SpbContext
)
{
	NTSTATUS status;
	ULONG attempt;

	if (!DevContext->HasResetGpio || DevContext->ResetGpio == NULL)
	{
		status = STATUS_NOT_SUPPORTED;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"TchResetControllerLocked requires reset GPIO - 0x%08lX",
			status);
		TchSetResetState(DevContext, TouchResetFailed, "TchResetControllerLocked");
		return status;
	}

	status = STATUS_UNSUCCESSFUL;
	TchSetRuntimeState(DevContext, TouchRuntimeResetting, "TchResetControllerLocked");

	for (attempt = 1; attempt <= TOUCH_CONTROLLER_RESET_ATTEMPTS; attempt++)
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_POWER,
			"TchResetControllerLocked attempt %lu/%lu",
			attempt,
			(ULONG)TOUCH_CONTROLLER_RESET_ATTEMPTS);

		TchSetResetState(DevContext, TouchResetAssertLow, "TchResetControllerLocked");
		status = SetResetGPIOLow(DevContext->ResetGpio);
		if (!NT_SUCCESS(status))
		{
			TchSetResetState(DevContext, TouchResetFailed, "TchResetControllerLocked");
			return status;
		}
		TchDelayMilliseconds(TOUCH_POWER_RAIL_STABLE_TIME / 1000);

		TchSetResetState(DevContext, TouchResetReleaseHigh, "TchResetControllerLocked");
		status = SetResetGPIOHigh(DevContext->ResetGpio);
		if (!NT_SUCCESS(status))
		{
			TchSetResetState(DevContext, TouchResetFailed, "TchResetControllerLocked");
			return status;
		}
		TchDelayMilliseconds(TOUCH_DELAY_TO_COMMUNICATE / 1000);

		TchSetResetState(DevContext, TouchResetWaitControllerReady, "TchResetControllerLocked");

		status = TchWaitForControllerReady(SpbContext);
		if (NT_SUCCESS(status))
		{
			TchSetResetState(DevContext, TouchResetComplete, "TchResetControllerLocked");
			return STATUS_SUCCESS;
		}

		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"TchResetControllerLocked controller-ready wait failed on attempt %lu - 0x%08lX",
			attempt,
			status);
	}

	TchSetResetState(DevContext, TouchResetFailed, "TchResetControllerLocked");
	return status;
}

static
NTSTATUS
TchReconfigureControllerAfterResetLocked(
	IN PDEVICE_EXTENSION DevContext,
	IN SPB_CONTEXT* SpbContext,
	IN PCSTR Source
)
{
	FTS521_CONTROLLER_CONTEXT* controllerContext;
	NTSTATUS status;

	controllerContext = (FTS521_CONTROLLER_CONTEXT*)DevContext->TouchContext;
	if (controllerContext == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot reconfigure controller after reset because touch context is NULL - 0x%08lX",
			Source,
			status);
		return status;
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s reconfiguring controller after reset",
		Source);

	status = Fts521ConfigureFunctions(controllerContext, SpbContext);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s failed to reconfigure controller functions after reset - 0x%08lX",
			Source,
			status);
		return status;
	}

	status = Fts521ConfigureInterruptEnable(controllerContext, SpbContext);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s failed to reconfigure controller interrupt source after reset - 0x%08lX",
			Source,
			status);
		return status;
	}

	status = TchFlushControllerFifo(SpbContext);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s failed to flush controller FIFO after reset reconfiguration - 0x%08lX",
			Source,
			status);
	}

	return status;
}

static
NTSTATUS
TchApplyConsoleDisplayStateLocked(
	IN PDEVICE_EXTENSION DevContext,
	IN SPB_CONTEXT* SpbContext,
	IN ULONG DisplayState,
	IN ULONG PreviousDisplayState,
	IN PCSTR Source
)
{
	BOOLEAN resetPerformed;
	NTSTATUS status;

	resetPerformed = FALSE;
	status = STATUS_SUCCESS;

	switch (DisplayState)
	{
	case 0:
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_POWER,
			"%s display off: setting low-power scan",
			Source);

		status = Fts521SetScanMode(SpbContext, SCAN_MODE_LOW_POWER, 0x00);
		if (NT_SUCCESS(status))
		{
			TchSetRuntimeState(DevContext, TouchRuntimeDisplayOff, Source);
		}
		else
		{
			TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
		}
		break;

	case 1:
	case 2:
		if (DisplayState == 1)
		{
			Trace(
				TRACE_LEVEL_INFORMATION,
				TRACE_POWER,
				"%s display on: restoring active scan",
				Source);
		}
		else
		{
			Trace(
				TRACE_LEVEL_INFORMATION,
				TRACE_POWER,
				"%s display dimmed: restoring active scan",
				Source);
		}

		if (DevContext->RuntimeState == TouchRuntimeDisplayOff ||
			PreviousDisplayState == 0)
		{
			status = TchResetControllerLocked(DevContext, SpbContext);
			if (!NT_SUCCESS(status))
			{
				TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
				break;
			}
			resetPerformed = TRUE;
		}

		if (resetPerformed)
		{
			status = TchReconfigureControllerAfterResetLocked(
				DevContext,
				SpbContext,
				Source);
			if (!NT_SUCCESS(status))
			{
				TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
				break;
			}
		}
		else
		{
			status = Fts521SetScanMode(SpbContext, SCAN_MODE_ACTIVE, SCAN_MODE_ACTIVE_SETTINGS_ALL);
			if (!NT_SUCCESS(status))
			{
				TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
				break;
			}
		}

		TchSetRuntimeState(DevContext, TouchRuntimeD0Active, Source);
		TchSetResetState(DevContext, TouchResetIdle, Source);
		break;

	default:
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s unknown display state - 0x%02X",
			Source,
			DisplayState);
		status = STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}

static
NTSTATUS
TchApplyConsoleDisplayState(
	IN PDEVICE_EXTENSION DevContext,
	IN ULONG DisplayState,
	IN ULONG PreviousDisplayState,
	IN PCSTR Source
)
{
	FTS521_CONTROLLER_CONTEXT* controllerContext;
	NTSTATUS status;

	status = TchRequireDisplayStateHardwareReady(DevContext, Source);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	controllerContext = (FTS521_CONTROLLER_CONTEXT*)DevContext->TouchContext;
	if (controllerContext == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot apply display state because touch context is NULL - 0x%08lX",
			Source,
			status);
		TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
		return status;
	}

	status = TchAcquireControllerLock(controllerContext);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = TchApplyConsoleDisplayStateLocked(
		DevContext,
		&DevContext->I2CContext,
		DisplayState,
		PreviousDisplayState,
		Source);

	WdfWaitLockRelease(controllerContext->ControllerLock);
	return status;
}

static
NTSTATUS
TchQueueConsoleDisplayState(
	IN PDEVICE_EXTENSION DevContext,
	IN ULONG DisplayState,
	IN PCSTR Source
)
{
	BOOLEAN enqueueWorkItem;
	BOOLEAN queuedDisplayState;
	ULONG previousDisplayState;
	ULONG queueCount;
	ULONG sequence;
	NTSTATUS status;

	enqueueWorkItem = FALSE;
	queuedDisplayState = FALSE;
	queueCount = 0;

	if (DisplayState > 2)
	{
		status = STATUS_INVALID_PARAMETER;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s rejecting invalid display state %lu runtime=%s interrupt=%s reset=%s - 0x%08lX",
			Source,
			DisplayState,
			TchRuntimeStateName(DevContext->RuntimeState),
			TchInterruptStateName(DevContext->InterruptState),
			TchResetStateName(DevContext->ResetState),
			status);
		return status;
	}

	if (DevContext->DisplayStateWorkItem == NULL ||
		DevContext->DisplayStateLock == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot queue display state %lu because display worker is not initialized - 0x%08lX",
			Source,
			DisplayState,
			status);
		return status;
	}

	WdfWaitLockAcquire(DevContext->DisplayStateLock, NULL);

	previousDisplayState = DevContext->LastDisplayState;

	if (DevContext->RuntimeState == TouchRuntimeFailed)
	{
		status = STATUS_DEVICE_NOT_READY;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s rejecting display state %lu because runtime is FAILED last=%lu - 0x%08lX",
			Source,
			DisplayState,
			previousDisplayState,
			status);
		WdfWaitLockRelease(DevContext->DisplayStateLock);
		return status;
	}

	DevContext->LastDisplayState = DisplayState;
	DevContext->DisplayStateSequence++;
	sequence = DevContext->DisplayStateSequence;

	if (DevContext->DisplayStateAcceptingWork != FALSE &&
		DevContext->DisplayStateHardwareReady == FALSE)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s display state gate is inconsistent accepting=1 ready=0 state=%lu sequence=%lu runtime=%s - 0x%08lX",
			Source,
			DisplayState,
			sequence,
			TchRuntimeStateName(DevContext->RuntimeState),
			status);
	}
	else if (DevContext->DisplayStateAcceptingWork == FALSE)
	{
		status = STATUS_SUCCESS;
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_POWER,
			"%s recorded display state %lu previous=%lu sequence=%lu without hardware queue ready=%d runtime=%s",
			Source,
			DisplayState,
			previousDisplayState,
			sequence,
			DevContext->DisplayStateHardwareReady,
			TchRuntimeStateName(DevContext->RuntimeState));
	}
	else
	{
		status = TchAppendDisplayStateEventLocked(
			DevContext,
			DisplayState,
			previousDisplayState,
			sequence,
			&enqueueWorkItem,
			Source);
		if (NT_SUCCESS(status))
		{
			queuedDisplayState = TRUE;
			queueCount = DevContext->DisplayStateQueueCount;
		}
	}

	WdfWaitLockRelease(DevContext->DisplayStateLock);

	if (!NT_SUCCESS(status))
	{
		TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
		return status;
	}

	if (queuedDisplayState == FALSE)
	{
		return STATUS_SUCCESS;
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s queued display state %lu previous=%lu sequence=%lu enqueue=%d count=%lu runtime=%s interrupt=%s reset=%s",
		Source,
		DisplayState,
		previousDisplayState,
		sequence,
		enqueueWorkItem,
		queueCount,
		TchRuntimeStateName(DevContext->RuntimeState),
		TchInterruptStateName(DevContext->InterruptState),
		TchResetStateName(DevContext->ResetState));

	if (enqueueWorkItem)
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_POWER,
			"%s display state work item enqueued",
			Source);
	}

	return STATUS_SUCCESS;
}

NTSTATUS
TchCreateDisplayStateWorker(
	IN WDFDEVICE FxDevice
)
{
	PDEVICE_EXTENSION devContext;
	PDISPLAY_STATE_WORKITEM_CONTEXT workItemContext;
	WDF_OBJECT_ATTRIBUTES workItemAttributes;
	WDF_WORKITEM_CONFIG workItemConfig;
	NTSTATUS status;

	devContext = GetDeviceContext(FxDevice);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
		&workItemAttributes,
		DISPLAY_STATE_WORKITEM_CONTEXT);
	workItemAttributes.ParentObject = FxDevice;

	WDF_WORKITEM_CONFIG_INIT(&workItemConfig, TchDisplayStateWorkItem);

	status = WdfWorkItemCreate(
		&workItemConfig,
		&workItemAttributes,
		&devContext->DisplayStateWorkItem);

	if (!NT_SUCCESS(status))
	{
		devContext->DisplayStateWorkItem = NULL;
		return status;
	}

	workItemContext = GetDisplayStateWorkItemContext(devContext->DisplayStateWorkItem);
	workItemContext->FxDevice = FxDevice;

	return STATUS_SUCCESS;
}

VOID
TchFlushDisplayStateWorker(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
)
{
	if (DevContext->DisplayStateWorkItem == NULL)
	{
		return;
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s flushing display state worker",
		Source);

	WdfWorkItemFlush(DevContext->DisplayStateWorkItem);
}

NTSTATUS
TchStopDisplayStateHardwareWork(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
)
{
	NTSTATUS status;

	if (DevContext->DisplayStateLock == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot stop display hardware work because display lock is not initialized - 0x%08lX",
			Source,
			status);
		return status;
	}

	WdfWaitLockAcquire(DevContext->DisplayStateLock, NULL);
	DevContext->DisplayStateAcceptingWork = FALSE;
	WdfWaitLockRelease(DevContext->DisplayStateLock);

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s stopped accepting display hardware work",
		Source);

	return STATUS_SUCCESS;
}

NTSTATUS
TchDisableDisplayStateHardwareWork(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
)
{
	NTSTATUS status;

	if (DevContext->DisplayStateLock == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot disable display hardware work because display lock is not initialized - 0x%08lX",
			Source,
			status);
		return status;
	}

	WdfWaitLockAcquire(DevContext->DisplayStateLock, NULL);

	if (DevContext->DisplayStateQueueCount != 0 ||
		DevContext->DisplayStateWorkQueued != FALSE)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot disable display hardware work with pending queue count=%lu workQueued=%d runtime=%s - 0x%08lX",
			Source,
			DevContext->DisplayStateQueueCount,
			DevContext->DisplayStateWorkQueued,
			TchRuntimeStateName(DevContext->RuntimeState),
			status);
		WdfWaitLockRelease(DevContext->DisplayStateLock);
		TchSetRuntimeState(DevContext, TouchRuntimeFailed, Source);
		return status;
	}

	DevContext->DisplayStateAcceptingWork = FALSE;
	DevContext->DisplayStateHardwareReady = FALSE;
	status = STATUS_SUCCESS;

	WdfWaitLockRelease(DevContext->DisplayStateLock);

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"%s disabled display hardware work",
		Source);

	return status;
}

VOID
TchDisplayStateWorkItem(
	IN WDFWORKITEM WorkItem
)
{
	PDISPLAY_STATE_WORKITEM_CONTEXT workItemContext;
	PDEVICE_EXTENSION devContext;
	TOUCH_DISPLAY_STATE_EVENT displayEvent;
	ULONG displayState;
	ULONG previousDisplayState;
	ULONG sequence;
	NTSTATUS status;

	workItemContext = GetDisplayStateWorkItemContext(WorkItem);
	devContext = GetDeviceContext(workItemContext->FxDevice);

	for (;;)
	{
		WdfWaitLockAcquire(devContext->DisplayStateLock, NULL);

		if (devContext->DisplayStateQueueCount == 0)
		{
			devContext->DisplayStateWorkQueued = FALSE;
			WdfWaitLockRelease(devContext->DisplayStateLock);
			break;
		}

		displayEvent = devContext->DisplayStateQueue[devContext->DisplayStateQueueHead];
		devContext->DisplayStateQueueHead =
			(devContext->DisplayStateQueueHead + 1) % TOUCH_DISPLAY_STATE_QUEUE_DEPTH;
		devContext->DisplayStateQueueCount--;

		displayState = displayEvent.DisplayState;
		previousDisplayState = displayEvent.PreviousDisplayState;
		sequence = displayEvent.Sequence;

		WdfWaitLockRelease(devContext->DisplayStateLock);

		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_POWER,
			"TchDisplayStateWorkItem applying display state %lu previous=%lu sequence=%lu runtime=%s interrupt=%s reset=%s",
			displayState,
			previousDisplayState,
			sequence,
			TchRuntimeStateName(devContext->RuntimeState),
			TchInterruptStateName(devContext->InterruptState),
			TchResetStateName(devContext->ResetState));

		status = TchApplyConsoleDisplayState(
			devContext,
			displayState,
			previousDisplayState,
			"TchDisplayStateWorkItem");

		Trace(
			NT_SUCCESS(status) ? TRACE_LEVEL_INFORMATION : TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"TchDisplayStateWorkItem completed display state %lu previous=%lu sequence=%lu status=0x%08lX runtime=%s interrupt=%s reset=%s",
			displayState,
			previousDisplayState,
			sequence,
			status,
			TchRuntimeStateName(devContext->RuntimeState),
			TchInterruptStateName(devContext->InterruptState),
			TchResetStateName(devContext->ResetState));

		if (!NT_SUCCESS(status))
		{
			WdfWaitLockAcquire(devContext->DisplayStateLock, NULL);
			devContext->DisplayStateAcceptingWork = FALSE;
			devContext->DisplayStateHardwareReady = FALSE;
			devContext->DisplayStateQueueHead = 0;
			devContext->DisplayStateQueueCount = 0;
			devContext->DisplayStateWorkQueued = FALSE;
			WdfWaitLockRelease(devContext->DisplayStateLock);

			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_POWER,
				"TchDisplayStateWorkItem stopped display hardware work after failure sequence=%lu - 0x%08lX",
				sequence,
				status);
			break;
		}
	}
}

NTSTATUS
TchRestoreConsoleDisplayState(
	IN PDEVICE_EXTENSION DevContext,
	IN PCSTR Source
)
{
	FTS521_CONTROLLER_CONTEXT* controllerContext;
	ULONG displayState;
	ULONG appliedDisplayState;
	ULONG sequence;
	NTSTATUS status;

	displayState = TOUCH_DISPLAY_STATE_UNKNOWN;
	appliedDisplayState = 1;
	sequence = 0;

	status = TchPrepareDisplayStateRestore(
		DevContext,
		&displayState,
		&sequence,
		Source);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	controllerContext = (FTS521_CONTROLLER_CONTEXT*)DevContext->TouchContext;
	if (controllerContext == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"%s cannot restore display state because touch context is NULL - 0x%08lX",
			Source,
			status);
		TchDisableDisplayStateHardwareWork(DevContext, Source);
		return status;
	}

	status = TchAcquireControllerLock(controllerContext);
	if (!NT_SUCCESS(status))
	{
		TchDisableDisplayStateHardwareWork(DevContext, Source);
		return status;
	}

	appliedDisplayState = TchDisplayStateForD0Restore(displayState);
	status = TchApplyConsoleDisplayStateLocked(
		DevContext,
		&DevContext->I2CContext,
		appliedDisplayState,
		displayState,
		Source);

	WdfWaitLockRelease(controllerContext->ControllerLock);
	if (!NT_SUCCESS(status))
	{
		TchDisableDisplayStateHardwareWork(DevContext, Source);
		return status;
	}

	status = TchFinishDisplayStateRestore(
		DevContext,
		displayState,
		appliedDisplayState,
		sequence,
		Source);
	return status;
}

NTSTATUS
TchPowerSettingCallback(
	_In_ LPCGUID SettingGuid,
	_In_ PVOID Value,
	_In_ ULONG ValueLength,
	_Inout_opt_ PVOID Context
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_EXTENSION devContext = NULL;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"TchPowerSettingCallback - Entry");

	if (Context == NULL)
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"TchPowerSettingCallback: Context is NULL"
		);

		status = STATUS_INVALID_DEVICE_REQUEST;
		goto exit;
	}

	devContext = (PDEVICE_EXTENSION)Context;

	//
	// Power Source change
	//
	if (IsEqualGUID(&GUID_ACDC_POWER_SOURCE, SettingGuid))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"AC/DC power source callback is not supported by this controller");
		UNREFERENCED_PARAMETER(Value);
		UNREFERENCED_PARAMETER(ValueLength);
		status = STATUS_NOT_SUPPORTED;
		goto exit;
	}
	else if (IsEqualGUID(&GUID_CONSOLE_DISPLAY_STATE, SettingGuid))
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_POWER,
			"Monitor State Change Notification");

		if (ValueLength != sizeof(DWORD))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_POWER,
				"TchPowerSettingCallback: Unexpected value size."
			);

			status = STATUS_INVALID_DEVICE_REQUEST;
			goto exit;
		}

		DWORD DisplayState = *(DWORD*)Value;
		status = TchQueueConsoleDisplayState(
			devContext,
			DisplayState,
			"TchPowerSettingCallback");
		if (!NT_SUCCESS(status))
		{
			goto exit;
		}
	}

exit:

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"TchPowerSettingCallback - Exit - 0x%08lX",
	status);

	return status;
}

NTSTATUS 
TchWakeDevice(
   IN VOID *ControllerContext,
   IN SPB_CONTEXT *SpbContext
   )
/*++

Routine Description:

   Enables multi-touch scanning

Arguments:

   ControllerContext - Touch controller context
   
   SpbContext - A pointer to the current i2c context

Return Value:

   NTSTATUS indicating success or failure

--*/
{	
	FTS521_CONTROLLER_CONTEXT* controller;
	NTSTATUS status;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"TchWakeDevice - Entry");

	controller = (FTS521_CONTROLLER_CONTEXT*) ControllerContext;
	status = STATUS_SUCCESS;

	//
	// Check if we were already on
	//
	if (controller->DevicePowerState == PowerDeviceD0)
	{
		goto exit;
	}

	status = TchAcquireControllerLock(controller);
	if (!NT_SUCCESS(status))
	{
		goto exit;
	}

	//
	// Attempt to put the controller into operating mode 
	//
	status = Fts521ChangeSleepState(
		controller,
		SpbContext,
		FTS521_F01_DEVICE_CONTROL_SLEEP_MODE_OPERATING);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"Error waking touch controller - 0x%08lX",
			status);
	}
	else
	{
		controller->DevicePowerState = PowerDeviceD0;
	}

	WdfWaitLockRelease(controller->ControllerLock);

exit:

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"TchWakeDevice - Exit");

	return status;
}

NTSTATUS
TchStandbyDevice(
   IN VOID *ControllerContext,
   IN SPB_CONTEXT *SpbContext,
   IN VOID* ReportContext
   )
/*++

Routine Description:

   Disables multi-touch scanning to conserve power

Arguments:

   ControllerContext - Touch controller context
   
   SpbContext - A pointer to the current i2c context

Return Value:

   NTSTATUS indicating success or failure

--*/
{
	FTS521_CONTROLLER_CONTEXT* controller;
	NTSTATUS status;

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"TchStandbyDevice - Entry");

	controller = (FTS521_CONTROLLER_CONTEXT*) ControllerContext;

	//
	// Interrupts are now disabled but the ISR may still be
	// executing, so grab the controller lock to ensure ISR
	// is finished touching HW and controller state.
	//
	status = TchAcquireControllerLock(controller);
	if (!NT_SUCCESS(status))
	{
		goto exit;
	}

	//
	// Put the chip in sleep mode
	//
	status = Fts521ChangeSleepState(
		ControllerContext,
		SpbContext,
		FTS521_F01_DEVICE_CONTROL_SLEEP_MODE_SLEEPING);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_POWER,
			"Error sleeping touch controller - 0x%08lX",
			status);
		goto exit_release_lock;
	}

	controller->DevicePowerState = PowerDeviceD3;

	//
	// Invalidate state
	//
	((PREPORT_CONTEXT)ReportContext)->Cache.SlotValid = 0;
	((PREPORT_CONTEXT)ReportContext)->Cache.SlotDirty = 0;
	((PREPORT_CONTEXT)ReportContext)->Cache.DownCount = 0;
	((PREPORT_CONTEXT)ReportContext)->ButtonCache.ButtonSlots[0] = 0;
	((PREPORT_CONTEXT)ReportContext)->ButtonCache.ButtonSlots[1] = 0;
	((PREPORT_CONTEXT)ReportContext)->ButtonCache.ButtonSlots[2] = 0;

exit_release_lock:
	WdfWaitLockRelease(controller->ControllerLock);

exit:

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_POWER,
		"TchStandbyDevice - Exit");

	return status;
}
