/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Copyright (c) Bingxing Wang. All Rights Reserved.
	Copyright (c) LumiaWoA authors. All Rights Reserved.

	Module Name:

		spb.c

	Abstract:

		Contains all I2C-specific functionality

	Environment:

		Kernel mode

	Revision History:

--*/

#include <internal.h>
#include <controller.h>
#include "spb.h"
#include <../shared/spb.h>
#include <spb.tmh>

#define I2C_VERBOSE_LOGGING 1

/*
 * FingerTipS-Touch I2C API
*/

NTSTATUS
FtsWriteReadU8UX(
	IN SPB_CONTEXT* SpbContext,
	IN PVOID Address,
	IN ULONG AddressLength,
	IN PVOID Data,
	IN ULONG DataLength
)
{
	SPB_TRANSFER_LIST_AND_ENTRIES(2) sequence;
	WDF_MEMORY_DESCRIPTOR sequenceDescriptor;
	ULONG_PTR bytesReturned;
	NTSTATUS status;
	LONGLONG lockTimeout;
	WDF_REQUEST_SEND_OPTIONS requestOptions;

	if (SpbContext == NULL ||
		Address == NULL ||
		AddressLength == 0 ||
		Data == NULL ||
		DataLength == 0)
	{
		return STATUS_INVALID_PARAMETER;
	}

	lockTimeout = TOUCH_REL_TIMEOUT_MS(TOUCH_SPB_LOCK_TIMEOUT_MS);
	status = WdfWaitLockAcquire(SpbContext->SpbLock, &lockTimeout);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Timeout acquiring Spb lock for write/read after %lu ms - 0x%08lX",
			(ULONG)TOUCH_SPB_LOCK_TIMEOUT_MS,
			status);
		goto exit;
	}

	SPB_TRANSFER_LIST_INIT(&sequence.List, 2);
	sequence.List.Transfers[0] = SPB_TRANSFER_LIST_ENTRY_INIT_NON_PAGED(
		SpbTransferDirectionToDevice,
		0,
		Address,
		AddressLength);
	sequence.ExtraTransfers[0] = SPB_TRANSFER_LIST_ENTRY_INIT_NON_PAGED(
		SpbTransferDirectionFromDevice,
		0,
		Data,
		DataLength);

	WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&requestOptions, TOUCH_REL_TIMEOUT_MS(TOUCH_SPB_TRANSFER_TIMEOUT_MS));
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&sequenceDescriptor, &sequence, sizeof(sequence));
	bytesReturned = 0;

	status = WdfIoTargetSendIoctlSynchronously(
		SpbContext->SpbIoTarget,
		NULL,
		IOCTL_SPB_EXECUTE_SEQUENCE,
		&sequenceDescriptor,
		NULL,
		&requestOptions,
		&bytesReturned);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error executing Spb write/read sequence addressBytes=%lu dataBytes=%lu timeout=%lu ms - 0x%08lX",
			AddressLength,
			DataLength,
			(ULONG)TOUCH_SPB_TRANSFER_TIMEOUT_MS,
			status);
	}

exit_release_lock:
	WdfWaitLockRelease(SpbContext->SpbLock);

exit:
	return status;
}

NTSTATUS
SpbDoWriteDataSynchronously(
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR Address,
	IN PVOID Data,
	IN ULONG Length
)
/*++

  Routine Description:

	This helper routine abstracts creating and sending an I/O
	request (I2C Write) to the Spb I/O target.

  Arguments:

	SpbContext - Pointer to the current device context
	Address	- The I2C register address to write to
	Data	   - A buffer to receive the data at at the above address
	Length	 - The amount of data to be read from the above address

  Return Value:

	NTSTATUS Status indicating success or failure

--*/
{
	PUCHAR buffer;
	ULONG length;
	WDFMEMORY memory;
	WDF_MEMORY_DESCRIPTOR memoryDescriptor;
	NTSTATUS status;
	WDF_REQUEST_SEND_OPTIONS requestOptions;

	//
	// The address pointer and data buffer must be combined
	// into one contiguous buffer representing the write transaction.
	//
	length = Length + 1;
	memory = NULL;

	if (length > DEFAULT_SPB_BUFFER_SIZE)
	{
		status = WdfMemoryCreate(
			WDF_NO_OBJECT_ATTRIBUTES,
			NonPagedPool,
			TOUCH_POOL_TAG,
			length,
			&memory,
			&buffer);

		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_SPB,
				"Error allocating memory for Spb write - 0x%08lX",
				status);
			goto exit;
		}

		WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
			&memoryDescriptor,
			memory,
			NULL);
	}
	else
	{
		buffer = (PUCHAR)WdfMemoryGetBuffer(SpbContext->WriteMemory, NULL);

		WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
			&memoryDescriptor,
			(PVOID)buffer,
			length);
	}

	//
	// Transaction starts by specifying the address bytes
	//
	RtlCopyMemory(buffer, &Address, sizeof(Address));

	//
	// Address is followed by the data payload
	//
	if (Length > 0)
	{
		RtlCopyMemory((buffer + sizeof(Address)), Data, length - sizeof(Address));
	}

#if I2C_VERBOSE_LOGGING
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "I2CWRITE: LENGTH=%d", length);
	for (ULONG j = 0; j < length; j++)
	{
		UCHAR byte = *(buffer + j);
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, " %02hhX", byte);
	}
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "\n");
#endif

	WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&requestOptions, TOUCH_REL_TIMEOUT_MS(TOUCH_SPB_TRANSFER_TIMEOUT_MS));

	status = WdfIoTargetSendWriteSynchronously(
		SpbContext->SpbIoTarget,
		NULL,
		&memoryDescriptor,
		NULL,
		&requestOptions,
		NULL);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error writing %lu byte(s) to Spb after %lu ms timeout - 0x%08lX",
			length,
			(ULONG)TOUCH_SPB_TRANSFER_TIMEOUT_MS,
			status);
		goto exit;
	}

exit:

	if (NULL != memory)
	{
		WdfObjectDelete(memory);
	}

	return status;
}

NTSTATUS
SpbWriteDataSynchronously(
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR Address,
	IN PVOID Data,
	IN ULONG Length
)
/*++

  Routine Description:

	This routine abstracts creating and sending an I/O
	request (I2C Write) to the Spb I/O target and utilizes
	a helper routine to do work inside of locked code.

  Arguments:

	SpbContext - Pointer to the current device context
	Address	- The I2C register address to write to
	Data	   - A buffer to receive the data at at the above address
	Length	 - The amount of data to be read from the above address

  Return Value:

	NTSTATUS Status indicating success or failure

--*/
{
	NTSTATUS status;
	LONGLONG lockTimeout;

	lockTimeout = TOUCH_REL_TIMEOUT_MS(TOUCH_SPB_LOCK_TIMEOUT_MS);
	status = WdfWaitLockAcquire(SpbContext->SpbLock, &lockTimeout);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Timeout acquiring Spb lock for write after %lu ms - 0x%08lX",
			(ULONG)TOUCH_SPB_LOCK_TIMEOUT_MS,
			status);
		return status;
	}

	status = SpbDoWriteDataSynchronously(
		SpbContext,
		Address,
		Data,
		Length);

	WdfWaitLockRelease(SpbContext->SpbLock);

	return status;
}

NTSTATUS
SpbReadDataSynchronously(
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR Address,
	_In_reads_bytes_(Length) PVOID Data,
	IN ULONG Length
)
/*++

  Routine Description:

	This helper routine abstracts creating and sending an I/O
	request (I2C Read) to the Spb I/O target.

  Arguments:

	SpbContext - Pointer to the current device context
	Address	- The I2C register address to read from
	Data	   - A buffer to receive the data at at the above address
	Length	 - The amount of data to be read from the above address

  Return Value:

	NTSTATUS Status indicating success or failure

--*/
{
	NTSTATUS status;

	status = FtsWriteReadU8UX(
		SpbContext,
		&Address,
		sizeof(Address),
		Data,
		Length);
	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error reading %lu byte(s) from Spb register 0x%02X - 0x%08lX",
			Length,
			Address,
			status);
	}

#if I2C_VERBOSE_LOGGING
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "I2CREAD: LENGTH=%d", Length);
	for (ULONG j = 0; j < Length; j++)
	{
		UCHAR byte = *((PUCHAR)Data + j);
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, " %02hhX", byte);
	}
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "\n");
#endif

	return status;
}

VOID
SpbTargetDeinitialize(
	IN WDFDEVICE FxDevice,
	IN SPB_CONTEXT* SpbContext
)
/*++

  Routine Description:

	This helper routine is used to free any members added to the SPB_CONTEXT,
	note the SPB I/O target is parented to the device and will be
	closed and free'd when the device is removed.

  Arguments:

	FxDevice   - Handle to the framework device object
	SpbContext - Pointer to the current device context

  Return Value:

	NTSTATUS Status indicating success or failure

--*/
{
	UNREFERENCED_PARAMETER(FxDevice);
	UNREFERENCED_PARAMETER(SpbContext);

	//
	// Free any SPB_CONTEXT allocations here
	//
	if (SpbContext->SpbLock != NULL)
	{
		WdfObjectDelete(SpbContext->SpbLock);
	}

	if (SpbContext->ReadMemory != NULL)
	{
		WdfObjectDelete(SpbContext->ReadMemory);
	}

	if (SpbContext->WriteMemory != NULL)
	{
		WdfObjectDelete(SpbContext->WriteMemory);
	}
}

NTSTATUS
SpbTargetInitialize(
	IN WDFDEVICE FxDevice,
	IN SPB_CONTEXT* SpbContext
)
/*++

  Routine Description:

	This helper routine opens the Spb I/O target and
	initializes a request object used for the lifetime
	of communication between this driver and Spb.

  Arguments:

	FxDevice   - Handle to the framework device object
	SpbContext - Pointer to the current device context

  Return Value:

	NTSTATUS Status indicating success or failure

--*/
{
	WDF_OBJECT_ATTRIBUTES objectAttributes;
	WDF_IO_TARGET_OPEN_PARAMS openParams;
	UNICODE_STRING spbDeviceName;
	WCHAR spbDeviceNameBuffer[RESOURCE_HUB_PATH_SIZE];
	NTSTATUS status;

	WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
	objectAttributes.ParentObject = FxDevice;

	status = WdfIoTargetCreate(
		FxDevice,
		&objectAttributes,
		&SpbContext->SpbIoTarget);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error creating IoTarget object - 0x%08lX",
			status);

		WdfObjectDelete(SpbContext->SpbIoTarget);
		goto exit;
	}

	RtlInitEmptyUnicodeString(
		&spbDeviceName,
		spbDeviceNameBuffer,
		sizeof(spbDeviceNameBuffer));

	status = RESOURCE_HUB_CREATE_PATH_FROM_ID(
		&spbDeviceName,
		SpbContext->I2cResHubId.LowPart,
		SpbContext->I2cResHubId.HighPart);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error creating Spb resource hub path string - 0x%08lX",
			status);
		goto exit;
	}

	WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
		&openParams,
		&spbDeviceName,
		(GENERIC_READ | GENERIC_WRITE));

	openParams.ShareAccess = 0;
	openParams.CreateDisposition = FILE_OPEN;
	openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;

	status = WdfIoTargetOpen(SpbContext->SpbIoTarget, &openParams);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error opening Spb target for communication - 0x%08lX",
			status);
		goto exit;
	}

	//
	// Allocate some fixed-size buffers from NonPagedPool for typical
	// Spb transaction sizes to avoid pool fragmentation in most cases
	//
	status = WdfMemoryCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		NonPagedPool,
		TOUCH_POOL_TAG,
		DEFAULT_SPB_BUFFER_SIZE,
		&SpbContext->WriteMemory,
		NULL);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error allocating default memory for Spb write - 0x%08lX",
			status);
		goto exit;
	}

	status = WdfMemoryCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		NonPagedPool,
		TOUCH_POOL_TAG,
		DEFAULT_SPB_BUFFER_SIZE,
		&SpbContext->ReadMemory,
		NULL);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error allocating default memory for Spb read - 0x%08lX",
			status);
		goto exit;
	}

	//
	// Allocate a waitlock to guard access to the default buffers
	//
	status = WdfWaitLockCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		&SpbContext->SpbLock);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_SPB,
			"Error creating Spb Waitlock - 0x%08lX",
			status);
		goto exit;
	}

exit:

	if (!NT_SUCCESS(status))
	{
		SpbTargetDeinitialize(FxDevice, SpbContext);
	}

	return status;
}
