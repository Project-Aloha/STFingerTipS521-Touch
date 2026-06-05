/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Copyright (c) Bingxing Wang. All Rights Reserved.
	Copyright (c) LumiaWoA authors. All Rights Reserved.

	Module Name:

		idle.c

	Abstract:

		This file contains the declarations for Power Idle specific callbacks
		and function definitions

	Environment:

		Kernel mode

	Revision History:

--*/

#include <internal.h>
#include <controller.h>
#include <idle.h>
#include <idle.tmh>

static
PCSTR
TchIdleStateName(
	IN TOUCH_IDLE_STATE State
)
{
	switch (State)
	{
	case TouchIdleNone:
		return "NONE";
	case TouchIdleCallbackQueued:
		return "CALLBACK_QUEUED";
	case TouchIdleForwarding:
		return "FORWARDING";
	case TouchIdleCompletionRequested:
		return "COMPLETION_REQUESTED";
	case TouchIdleParked:
		return "PARKED";
	case TouchIdleCompleting:
		return "COMPLETING";
	default:
		return "UNKNOWN";
	}
}

NTSTATUS
TchProcessIdleRequest(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request,
	OUT BOOLEAN* Pending
)
/*++

Routine Description:

   Handles HIDClass's idle notification request.

   This request is provided to a HID miniport, and provides a callback
   routine typically used by HID miniports to self-manage power.

   The callback routine is invoked by the miniport to indicate that all
   peripherals are idle, and in response the HID class driver will:
	 1) Queue a wait/wake IRP to the device, and
	 2) Set the device to D3

   Invoking the callback allows HIDClass to power the stack down to D3. This
   driver must not invoke it unless a D3 wake path is implemented and verified.

   The Request will be completed when either HIDCLASS cancels it or
   there is a device wake signal that will cause us to complete it.

Arguments:

   Device - Handle to WDF Device Object

   Request - Handle to request object

   Pending - flag to monitor if the request was sent down the stack

Return Value:

   On success, the function returns STATUS_SUCCESS
   On failure it passes the relevant error code to the caller.

--*/
{
	PDEVICE_EXTENSION devContext;
	PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO idleCallbackInfo;
	PIRP irp;
	PIO_STACK_LOCATION irpSp;
	NTSTATUS status;
#if TOUCH_HID_IDLE_D3_WAKE_SUPPORTED != 0
	ULONG sequence;
#endif

	devContext = GetDeviceContext(Device);

	NT_ASSERT(Pending != NULL);
	*Pending = FALSE;

	//
	// Retrieve request parameters and validate
	//
	irp = WdfRequestWdmGetIrp(Request);
	irpSp = IoGetCurrentIrpStackLocation(irp);

	if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
		sizeof(HID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO))
	{
		status = STATUS_INVALID_BUFFER_SIZE;

		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_HID,
			"Error: Input buffer is too small to process idle request - 0x%08lX", 
			status);

		goto exit;
	}

	//
	// Grab the callback
	//
	idleCallbackInfo = (PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO)
		irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

	NT_ASSERT(idleCallbackInfo != NULL);

	if (idleCallbackInfo == NULL || idleCallbackInfo->IdleCallback == NULL)
	{
		status = STATUS_NO_CALLBACK_ACTIVE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_HID,
			"Error: Idle Notification request %p has no idle callback info - 0x%08lX",
			Request,
			status);
		goto exit;
	}

	if (devContext->IdleLock == NULL)
	{
		status = STATUS_INVALID_DEVICE_STATE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_HID,
			"Error: Idle state lock is not initialized for request %p - 0x%08lX",
			Request,
			status);
		goto exit;
	}

#if TOUCH_HID_IDLE_D3_WAKE_SUPPORTED == 0
	status = STATUS_NOT_SUPPORTED;
	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_HID,
		"Rejecting idle notification request %p because HID idle D3 wake is not implemented - 0x%08lX",
		Request,
		status);
	goto exit;
#else
	WdfWaitLockAcquire(devContext->IdleLock, NULL);
	if (devContext->IdleState != TouchIdleNone ||
		devContext->IdleRequest != NULL)
	{
		status = STATUS_DEVICE_BUSY;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_HID,
			"Rejecting duplicate idle request %p currentRequest=%p state=%s sequence=%lu - 0x%08lX",
			Request,
			devContext->IdleRequest,
			TchIdleStateName(devContext->IdleState),
			devContext->IdleSequence,
			status);
		WdfWaitLockRelease(devContext->IdleLock);
		goto exit;
	}

	devContext->IdleSequence++;
	sequence = devContext->IdleSequence;
	devContext->IdleRequest = Request;
	devContext->IdleState = TouchIdleCallbackQueued;
	devContext->IdleCompletionStatus = STATUS_SUCCESS;
	WdfWaitLockRelease(devContext->IdleLock);

	{
		//
		// Create a workitem for the idle callback
		//
		WDF_OBJECT_ATTRIBUTES workItemAttributes;
		WDF_WORKITEM_CONFIG workitemConfig;
		WDFWORKITEM idleWorkItem;
		PIDLE_WORKITEM_CONTEXT idleWorkItemContext;

		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&workItemAttributes, IDLE_WORKITEM_CONTEXT);
		workItemAttributes.ParentObject = devContext->FxDevice;

		WDF_WORKITEM_CONFIG_INIT(&workitemConfig, TchIdleIrpWorkitem);

		status = WdfWorkItemCreate(
			&workitemConfig,
			&workItemAttributes,
			&idleWorkItem
		);

		if (!NT_SUCCESS(status)) {
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_HID,
				"Error creating creating idle work item - 0x%08lX",
				status);

			WdfWaitLockAcquire(devContext->IdleLock, NULL);
			if (devContext->IdleRequest == Request)
			{
				devContext->IdleRequest = NULL;
				devContext->IdleState = TouchIdleNone;
				devContext->IdleCompletionStatus = STATUS_SUCCESS;
			}
			WdfWaitLockRelease(devContext->IdleLock);

			goto exit;
		}

		//
		// Set the workitem context
		//
		idleWorkItemContext = GetWorkItemContext(idleWorkItem);
		idleWorkItemContext->FxDevice = devContext->FxDevice;
		idleWorkItemContext->FxRequest = Request;

		*Pending = TRUE;

		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_IDLE,
			"Queued idle callback work item for Request:%p callback:%p context:%p sequence=%lu",
			Request,
			idleCallbackInfo->IdleCallback,
			idleCallbackInfo->IdleContext,
			sequence);

		//
		// Enqueue a workitem for the idle callback
		//
		WdfWorkItemEnqueue(idleWorkItem);
	}
#endif

exit:

	return status;
}

VOID
TchIdleIrpWorkitem(
	IN WDFWORKITEM IdleWorkItem
)
/*++

Routine Description:

	This is a workitem routine that TchProcessIdleRequest queues when
	handling the HIDClass's idle notification IRP, so the idle callback can be made in
	a different thread context, instead of the Idle Irp's dispatch call.

Arguments:

	IdleWorkItem	-   Handle to a WDF workitem object

Return Value:

	VOID

--*/
{
#if TOUCH_HID_IDLE_D3_WAKE_SUPPORTED == 0
	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_IDLE,
		"HID idle work item reached while HID idle D3 wake support is disabled");

	WdfObjectDelete(IdleWorkItem);
	return;
#else
	NTSTATUS status;
	PIDLE_WORKITEM_CONTEXT idleWorkItemContext;
	PDEVICE_EXTENSION deviceContext;
	PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO idleCallbackInfo;
	BOOLEAN completeRequest;
	BOOLEAN forwardRequest;
	BOOLEAN completeAfterForward;
	NTSTATUS completionStatus;
	ULONG sequence;
	TOUCH_IDLE_STATE oldState;

	idleWorkItemContext = GetWorkItemContext(IdleWorkItem);
	NT_ASSERT(idleWorkItemContext != NULL);

	deviceContext = GetDeviceContext(idleWorkItemContext->FxDevice);
	NT_ASSERT(deviceContext != NULL);

	//
	// Get the idle callback info from the workitem context
	//
	idleCallbackInfo = (PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO)
		IoGetCurrentIrpStackLocation(WdfRequestWdmGetIrp(idleWorkItemContext->FxRequest))->\
		Parameters.DeviceIoControl.Type3InputBuffer;

	//
	// idleCallbackInfo is validated already, so invoke idle callback
	//
	idleCallbackInfo->IdleCallback(idleCallbackInfo->IdleContext);

	completeRequest = FALSE;
	forwardRequest = FALSE;
	completeAfterForward = FALSE;
	completionStatus = STATUS_SUCCESS;
	sequence = 0;
	oldState = TouchIdleNone;

	WdfWaitLockAcquire(deviceContext->IdleLock, NULL);

	if (deviceContext->IdleRequest != idleWorkItemContext->FxRequest)
	{
		status = STATUS_CANCELLED;
		completeRequest = TRUE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_IDLE,
			"Idle callback work item found stale request %p currentRequest=%p state=%s - 0x%08lX",
			idleWorkItemContext->FxRequest,
			deviceContext->IdleRequest,
			TchIdleStateName(deviceContext->IdleState),
			status);
	}
	else if (deviceContext->IdleState == TouchIdleCompletionRequested)
	{
		status = deviceContext->IdleCompletionStatus;
		sequence = deviceContext->IdleSequence;
		deviceContext->IdleRequest = NULL;
		deviceContext->IdleState = TouchIdleNone;
		deviceContext->IdleCompletionStatus = STATUS_SUCCESS;
		completeRequest = TRUE;
	}
	else if (deviceContext->IdleState == TouchIdleCallbackQueued)
	{
		status = STATUS_SUCCESS;
		sequence = deviceContext->IdleSequence;
		deviceContext->IdleState = TouchIdleForwarding;
		forwardRequest = TRUE;
	}
	else
	{
		status = STATUS_INVALID_DEVICE_STATE;
		sequence = deviceContext->IdleSequence;
		oldState = deviceContext->IdleState;
		deviceContext->IdleRequest = NULL;
		deviceContext->IdleState = TouchIdleNone;
		deviceContext->IdleCompletionStatus = STATUS_SUCCESS;
		completeRequest = TRUE;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_IDLE,
			"Idle callback work item found unexpected state=%s for Request:%p sequence=%lu - 0x%08lX",
			TchIdleStateName(oldState),
			idleWorkItemContext->FxRequest,
			sequence,
			status);
	}

	WdfWaitLockRelease(deviceContext->IdleLock);

	if (completeRequest)
	{
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_IDLE,
			"Completing idle Request:%p immediately after callback sequence=%lu - 0x%08lX",
			idleWorkItemContext->FxRequest,
			sequence,
			status);
		WdfRequestComplete(idleWorkItemContext->FxRequest, status);
		goto exit;
	}

	if (forwardRequest)
	{
		status = WdfRequestForwardToIoQueue(
			idleWorkItemContext->FxRequest,
			deviceContext->IdleQueue);
	}

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_IDLE,
			"Error forwarding idle notification Request:%p to IdleQueue:%p sequence=%lu - 0x%08lX",
			idleWorkItemContext->FxRequest,
			deviceContext->IdleQueue,
			sequence,
			status);

		WdfWaitLockAcquire(deviceContext->IdleLock, NULL);
		if (deviceContext->IdleRequest == idleWorkItemContext->FxRequest)
		{
			deviceContext->IdleRequest = NULL;
			deviceContext->IdleState = TouchIdleNone;
			deviceContext->IdleCompletionStatus = STATUS_SUCCESS;
		}
		WdfWaitLockRelease(deviceContext->IdleLock);

		WdfRequestComplete(idleWorkItemContext->FxRequest, status);
	}
	else
	{
		WdfWaitLockAcquire(deviceContext->IdleLock, NULL);
		if (deviceContext->IdleRequest == idleWorkItemContext->FxRequest &&
			deviceContext->IdleState == TouchIdleCompletionRequested)
		{
			completionStatus = deviceContext->IdleCompletionStatus;
			completeAfterForward = TRUE;
		}

		if (deviceContext->IdleRequest == idleWorkItemContext->FxRequest)
		{
			deviceContext->IdleState = TouchIdleParked;
		}
		WdfWaitLockRelease(deviceContext->IdleLock);

		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_IDLE,
			"Forwarded idle notification Request:%p to IdleQueue:%p sequence=%lu completeAfterForward=%d - 0x%08lX",
			idleWorkItemContext->FxRequest,
			deviceContext->IdleQueue,
			sequence,
			completeAfterForward,
			status);

		if (completeAfterForward)
		{
			TchCompleteIdleIrpWithStatus(
				deviceContext,
				completionStatus,
				"TchIdleIrpWorkitem");
		}
	}

exit:
	//
	// Delete the workitem since we're done with it
	//
	WdfObjectDelete(IdleWorkItem);

	return;
#endif
}


VOID
TchCompleteIdleIrp(
	IN PDEVICE_EXTENSION FxDeviceContext
)
/*++

Routine Description:

	This is invoked when we enter D0.
	We simply complete the Idle Irp if it hasn't been cancelled already.

Arguments:

	FxDeviceContext -  Pointer to Device Context for the device

Return Value:



--*/
{
	TchCompleteIdleIrpWithStatus(
		FxDeviceContext,
		STATUS_SUCCESS,
		"TchCompleteIdleIrp");
}

VOID
TchCompleteIdleIrpWithStatus(
	IN PDEVICE_EXTENSION FxDeviceContext,
	IN NTSTATUS CompletionStatus,
	IN PCSTR Source
)
{
	NTSTATUS status;
	WDFREQUEST expectedRequest;
	WDFREQUEST request;
	TOUCH_IDLE_STATE idleState;
	ULONG sequence;

	if (FxDeviceContext->IdleLock == NULL)
	{
		return;
	}

	expectedRequest = NULL;
	request = NULL;
	sequence = 0;

	WdfWaitLockAcquire(FxDeviceContext->IdleLock, NULL);
	idleState = FxDeviceContext->IdleState;
	expectedRequest = FxDeviceContext->IdleRequest;
	sequence = FxDeviceContext->IdleSequence;

	switch (idleState)
	{
	case TouchIdleNone:
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_IDLE,
			"%s found no pending idle request sequence=%lu",
			Source,
			sequence);
		WdfWaitLockRelease(FxDeviceContext->IdleLock);
		return;

	case TouchIdleCallbackQueued:
	case TouchIdleForwarding:
		FxDeviceContext->IdleState = TouchIdleCompletionRequested;
		FxDeviceContext->IdleCompletionStatus = CompletionStatus;
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_IDLE,
			"%s deferred idle completion for Request:%p state=%s sequence=%lu completion=0x%08lX",
			Source,
			expectedRequest,
			TchIdleStateName(idleState),
			sequence,
			CompletionStatus);
		WdfWaitLockRelease(FxDeviceContext->IdleLock);
		return;

	case TouchIdleCompletionRequested:
	case TouchIdleCompleting:
		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_IDLE,
			"%s idle request already completing Request:%p state=%s sequence=%lu",
			Source,
			expectedRequest,
			TchIdleStateName(idleState),
			sequence);
		WdfWaitLockRelease(FxDeviceContext->IdleLock);
		return;

	case TouchIdleParked:
		FxDeviceContext->IdleState = TouchIdleCompleting;
		break;

	default:
		FxDeviceContext->IdleRequest = NULL;
		FxDeviceContext->IdleState = TouchIdleNone;
		FxDeviceContext->IdleCompletionStatus = STATUS_SUCCESS;
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_IDLE,
			"%s resetting unknown idle state=%d Request:%p sequence=%lu",
			Source,
			idleState,
			expectedRequest,
			sequence);
		WdfWaitLockRelease(FxDeviceContext->IdleLock);
		return;
	}

	WdfWaitLockRelease(FxDeviceContext->IdleLock);

	status = WdfIoQueueRetrieveNextRequest(
		FxDeviceContext->IdleQueue,
		&request);

	if (NT_SUCCESS(status) && request != NULL)
	{
		WdfWaitLockAcquire(FxDeviceContext->IdleLock, NULL);
		if (FxDeviceContext->IdleRequest == request)
		{
			FxDeviceContext->IdleRequest = NULL;
			FxDeviceContext->IdleState = TouchIdleNone;
			FxDeviceContext->IdleCompletionStatus = STATUS_SUCCESS;
		}
		WdfWaitLockRelease(FxDeviceContext->IdleLock);

		Trace(
			TRACE_LEVEL_INFORMATION,
			TRACE_IDLE,
			"%s completed idle Request:%p from IdleQueue:%p sequence=%lu completion=0x%08lX",
			Source,
			request,
			FxDeviceContext->IdleQueue,
			sequence,
			CompletionStatus);

		WdfRequestComplete(request, CompletionStatus);
		return;
	}

	WdfWaitLockAcquire(FxDeviceContext->IdleLock, NULL);
	if (FxDeviceContext->IdleRequest == expectedRequest &&
		FxDeviceContext->IdleState == TouchIdleCompleting)
	{
		FxDeviceContext->IdleRequest = NULL;
		FxDeviceContext->IdleState = TouchIdleNone;
		FxDeviceContext->IdleCompletionStatus = STATUS_SUCCESS;
	}
	WdfWaitLockRelease(FxDeviceContext->IdleLock);

	Trace(
		TRACE_LEVEL_WARNING,
		TRACE_IDLE,
		"%s could not retrieve idle Request:%p from IdleQueue:%p sequence=%lu retrieve=0x%08lX completion=0x%08lX",
		Source,
		expectedRequest,
		FxDeviceContext->IdleQueue,
		sequence,
		status,
		CompletionStatus);
}

VOID
TchIdleRequestCanceledOnQueue(
	IN WDFQUEUE Queue,
	IN WDFREQUEST Request
)
{
	PDEVICE_EXTENSION devContext;
	WDFDEVICE device;
	TOUCH_IDLE_STATE oldState;
	ULONG sequence;

	device = WdfIoQueueGetDevice(Queue);
	devContext = GetDeviceContext(device);
	oldState = TouchIdleNone;
	sequence = 0;

	if (devContext->IdleLock != NULL)
	{
		WdfWaitLockAcquire(devContext->IdleLock, NULL);
		oldState = devContext->IdleState;
		sequence = devContext->IdleSequence;

		if (devContext->IdleRequest == Request)
		{
			devContext->IdleRequest = NULL;
			devContext->IdleState = TouchIdleNone;
			devContext->IdleCompletionStatus = STATUS_SUCCESS;
		}

		WdfWaitLockRelease(devContext->IdleLock);
	}

	Trace(
		TRACE_LEVEL_INFORMATION,
		TRACE_IDLE,
		"Idle request canceled on queue Request:%p oldState=%s sequence=%lu",
		Request,
		TchIdleStateName(oldState),
		sequence);

	WdfRequestComplete(Request, STATUS_CANCELLED);
}
