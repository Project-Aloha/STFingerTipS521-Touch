/*++
      Copyright (c) Microsoft Corporation. All Rights Reserved.
      Sample code. Dealpoint ID #843729.

      Module Name:

            ftsinternal.c

      Abstract:

            Contains FingerTipS521 initialization code

      Environment:

            Kernel mode

      Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <report.h>
#include <fts521\ftsinternal.h>
#include <ftsinternal.tmh>

BYTE FTS521_READ_EVENTS[1] = { 0x86 };

BYTE eventbuf[256];

NTSTATUS
Fts521BuildFunctionsTable(
      IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
      IN SPB_CONTEXT* SpbContext
)
{
      UNREFERENCED_PARAMETER(SpbContext);
      UNREFERENCED_PARAMETER(ControllerContext);

      return STATUS_SUCCESS;
}

NTSTATUS
Fts521ChangePage(
      IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
      IN SPB_CONTEXT* SpbContext,
      IN int DesiredPage
)
{
      UNREFERENCED_PARAMETER(SpbContext);
      UNREFERENCED_PARAMETER(ControllerContext);
      UNREFERENCED_PARAMETER(DesiredPage);

      return STATUS_SUCCESS;
}

NTSTATUS
Fts521ConfigureFunctions(
      IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
      IN SPB_CONTEXT* SpbContext
)
{
    NTSTATUS status;
    LARGE_INTEGER delay;

    FTS521_CONTROLLER_CONTEXT* controller;
    controller = (FTS521_CONTROLLER_CONTEXT*)ControllerContext;

    BYTE FTS521_LOCKDOWN[3] = { 0xA4, 0x06, 0x70 };
    status = FtsWrite(SpbContext, FTS521_LOCKDOWN, 3);
    if (!NT_SUCCESS(status))
    {
      Trace(
          TRACE_LEVEL_ERROR,
          TRACE_INTERRUPT,
          "Writing Lockdown code into the IC done");
    }

    //Active Scan OFF
    SetScanMode(controller->FxDevice, SpbContext, SCAN_MODE_ACTIVE, 0x00);

    delay.QuadPart = RELATIVE(MILLISECONDS(50));
    KeDelayExecutionThread(KernelMode, TRUE, &delay);

    //Active Scan ON
    SetScanMode(controller->FxDevice, SpbContext, SCAN_MODE_ACTIVE, 0x01);

    return STATUS_SUCCESS;
}

NTSTATUS
Fts521GetObjectStatusFromController(
      IN VOID* ControllerContext,
      IN SPB_CONTEXT* SpbContext,
      IN DETECTED_OBJECTS* Data
)
/*++

Routine Description:

      This routine reads raw touch messages from hardware. If there is
      no touch data available (if a non-touch interrupt fired), the
      function will not return success and no touch data was transferred.

Arguments:

      ControllerContext - Touch controller context
      SpbContext - A pointer to the current i2c context
      Data - A pointer to any returned F11 touch data

Return Value:

      NTSTATUS, where only success indicates data was returned

--*/
{
      NTSTATUS              status;
      BYTE                  touchType;
      BYTE                  touchId;
      int                   remain = 0;
      int                   x = 0;
      int                   y = 0;
      int                   base = 0;

      FTS521_CONTROLLER_CONTEXT* controller;

      PFOCAL_TECH_EVENT_DATA controllerData = NULL;
      controller = (FTS521_CONTROLLER_CONTEXT*)ControllerContext;

      status = FtsWriteReadU8UX(SpbContext, FTS521_READ_EVENTS, &eventbuf[0], 3, 8);

      if (!NT_SUCCESS(status))
      {
            Trace(
                  TRACE_LEVEL_ERROR,
                  TRACE_INTERRUPT,
                  "Error reading finger status data - 0x%08lX",
                  status);

            goto exit;
      }

      remain = eventbuf[7];
      if (remain > 0)
      {
            FtsWriteReadU8UX(SpbContext, FTS521_READ_EVENTS, &eventbuf[8], 3, 10);
      }

      for (int i = 0; i < remain+1 ; i++)
      {
            base = i * 8;

            touchType = eventbuf[base + 1] & 0x0F;
            touchId = (eventbuf[base + 1] & 0xF0) >> 4;

            x = ((eventbuf[base + 3] & 0x0F) << 8) | (eventbuf[base + 2]);
            y = (eventbuf[base + 4] << 4) | ((eventbuf[base + 3] & 0xF0) >> 4);

            /* event[0] */
            switch (eventbuf[base + 0])
            {
                  case EVT_ID_NOEVENT:
                  /*
                  * If there is no event, then do not input its xy coordinates.
                  */
                        break;
                  case EVT_ID_ENTER_POINT:
                  case EVT_ID_MOTION_POINT:
                        Data->States[touchId] = OBJECT_STATE_FINGER_PRESENT_WITH_ACCURATE_POS;
                        break;
                  case EVT_ID_LEAVE_POINT:
                        Data->States[touchId] = OBJECT_STATE_NOT_PRESENT;
                        break;
            }
        
            Data->Positions[touchId].X = x;
            Data->Positions[touchId].Y = y;

            /* event[1]
            *  TODO: Need to complete the handling methods for the 
            *        following errors in the future.
            */
            switch (eventbuf[i * 8 + 1])
            {
                  case EVT_TYPE_ERROR_ESD:
                  case EVT_TYPE_ERROR_WATCHDOG:
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
      NTSTATUS status = STATUS_SUCCESS;
      DETECTED_OBJECTS data;

      RtlZeroMemory(&data, sizeof(data));

      //
      // See if new touch data is available
      //

      status = Fts521GetObjectStatusFromController(
            ControllerContext,
            SpbContext,
            &data
      );

      if (!NT_SUCCESS(status))
      {
            Trace(
                  TRACE_LEVEL_VERBOSE,
                  TRACE_SAMPLES,
                  "No object data to report - 0x%08lX",
                  status);

            goto exit;
      }

      status = ReportObjects(
            ReportContext,
            data);

      if (!NT_SUCCESS(status))
      {
            Trace(
                  TRACE_LEVEL_VERBOSE,
                  TRACE_SAMPLES,
                  "Error while reporting objects - 0x%08lX",
                  status);

            goto exit;
      }

exit:
      return status;
}


NTSTATUS
Fts521ServiceInterrupts(
      IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
      IN SPB_CONTEXT* SpbContext,
      IN PREPORT_CONTEXT ReportContext
)
{
      NTSTATUS status = STATUS_SUCCESS;

      TchServiceObjectInterrupts(ControllerContext, SpbContext, ReportContext);

      return status;
}

NTSTATUS
Fts521SetReportingFlags(
    IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
    IN SPB_CONTEXT* SpbContext,
    IN UCHAR NewMode,
    OUT UCHAR* OldMode
)
{
      UNREFERENCED_PARAMETER(SpbContext);
      UNREFERENCED_PARAMETER(ControllerContext);
      UNREFERENCED_PARAMETER(NewMode);
      UNREFERENCED_PARAMETER(OldMode);

      return STATUS_SUCCESS;
}

NTSTATUS
Fts521ChangeChargerConnectedState(
    IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
    IN SPB_CONTEXT* SpbContext,
    IN UCHAR ChargerConnectedState
)
{
      UNREFERENCED_PARAMETER(SpbContext);
      UNREFERENCED_PARAMETER(ControllerContext);
      UNREFERENCED_PARAMETER(ChargerConnectedState);

      return STATUS_SUCCESS;
}

NTSTATUS
Fts521ChangeSleepState(
    IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
    IN SPB_CONTEXT* SpbContext,
    IN UCHAR SleepState
)
{
      UNREFERENCED_PARAMETER(SpbContext);
      UNREFERENCED_PARAMETER(ControllerContext);
      UNREFERENCED_PARAMETER(SleepState);

      return STATUS_SUCCESS;
}

NTSTATUS
Fts521CheckInterrupts(
    IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
    IN SPB_CONTEXT* SpbContext,
    IN ULONG* InterruptStatus
)
{
      UNREFERENCED_PARAMETER(SpbContext);
      UNREFERENCED_PARAMETER(ControllerContext);
      UNREFERENCED_PARAMETER(InterruptStatus);

      return STATUS_SUCCESS;
}

NTSTATUS
Fts521ConfigureInterruptEnable(
    IN FTS521_CONTROLLER_CONTEXT* ControllerContext,
    IN SPB_CONTEXT* SpbContext
)
{
      UNREFERENCED_PARAMETER(SpbContext);
      UNREFERENCED_PARAMETER(ControllerContext);

      return STATUS_SUCCESS;
}