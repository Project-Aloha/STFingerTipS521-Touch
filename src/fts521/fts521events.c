/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Sample code. Dealpoint ID #843729.

	Module Name:

		fts521events.c

	Abstract:

		Contains STFingerTipSTouch initialization code

	Environment:

		Kernel mode

	Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <spb.h>
#include <report.h>
#include <fts521\fts521internal.h>
#include <fts521\fts521regs.h>
#include <fts521\fts521pointer.h>
#include <fts521events.tmh>

NTSTATUS
Fts521ProcessEvent(
	FTS521_CONTROLLER_CONTEXT* ControllerContext,
	PREPORT_CONTEXT ReportContext,
	BYTE* EventData
)
{
	NTSTATUS status = STATUS_SUCCESS;

	int EventID;

	EventID = EventData[0];

	switch (EventID)
	{
		case EVT_ID_ENTER_POINT:
		case EVT_ID_MOTION_POINT:
		{
			status = Fts521ProcessEnterOrMotionPointerEvent(ControllerContext, ReportContext, EventData);
			if (!NT_SUCCESS(status))
			{
				Trace(
					TRACE_LEVEL_VERBOSE,
					TRACE_SAMPLES,
					"Fts521ProcessEvent - Error while processing enter or motion pointer event - 0x%08lX",
					status);

				goto exit;
			}

			break;
		}
		case EVT_ID_LEAVE_POINT:
		{
			status = Fts521ProcessLeavePointerEvent(ControllerContext, ReportContext, EventData);
			if (!NT_SUCCESS(status))
			{
				Trace(
					TRACE_LEVEL_VERBOSE,
					TRACE_SAMPLES,
					"Fts521ProcessEvent - Error while processing leave pointer event - 0x%08lX",
					status);
				goto exit;
			}

			break;
		}
		default:
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_REPORTING,
				"Fts521ProcessEvent - Unknown event id %d",
				EventID);
			break;
		}
	}

exit:
	return status;
}

NTSTATUS
TchServiceObjectInterrupts(
	IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN PREPORT_CONTEXT ReportContext
)
{
	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"TchServiceObjectInterrupts - Entry");

	NTSTATUS status = STATUS_SUCCESS;
	FTS521_CONTROLLER_CONTEXT* controller;
	controller = (FTS521_CONTROLLER_CONTEXT*)ControllerContext;

	DETECTED_OBJECTS data;
	RtlZeroMemory(&data, sizeof(data));

	int  i = 0;
	int  remain = 0;
	BYTE EventBuffer[256];
	BYTE FTS521_READ_EVENTS[1] = { FIFO_CMD_READONE };

	status = FtsWriteReadU8UX(SpbContext, FTS521_READ_EVENTS, 1, &EventBuffer[0], FIFO_EVENT_SIZE);

	if (!NT_SUCCESS(status))
	{
		Trace(
			TRACE_LEVEL_ERROR,
			TRACE_INTERRUPT,
			"Error reading finger status data - 0x%08lX",
			status);

		goto exit;
	}

	remain = EventBuffer[7];
	if (remain > FIFO_MAX_REMAIN_EVENTS)
	{
		remain = FIFO_MAX_REMAIN_EVENTS;
	}

	if (remain > 0)
	{
		status = FtsWriteReadU8UX(SpbContext, FTS521_READ_EVENTS, 1, &EventBuffer[FIFO_EVENT_SIZE], remain * FIFO_EVENT_SIZE);
		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_INTERRUPT,
				"Error reading remaining touch events - 0x%08lX",
				status);

			goto exit;
		}
	}

	for (int CurrentEventId = 0; CurrentEventId < remain + 1; CurrentEventId++)
	{
		i = CurrentEventId * FIFO_EVENT_SIZE;

		// Process the event
		status = Fts521ProcessEvent(controller, ReportContext, EventBuffer + i);
		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_INTERRUPT,
				"TchServiceObjectInterrupts - Error processing event %d - 0x%08lX",
				CurrentEventId,
				status);

			goto exit;
		}
	}

exit:

	Trace(
		TRACE_LEVEL_ERROR,
		TRACE_REPORTING,
		"TchServiceObjectInterrupts - Exit\n");

	return status;
}
