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
#include <spb.h>
#include <fts521\ftsinternal.h>
#include <internal.h>
#include <touch_power\touch_power.h>
#include <power.tmh>

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
    FTS521_CONTROLLER_CONTEXT* ControllerContext = NULL;
    SPB_CONTEXT* SpbContext = NULL;

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
    ControllerContext = (FTS521_CONTROLLER_CONTEXT*)devContext->TouchContext;
    SpbContext = &(devContext->I2CContext);

    //
    // Power Source change
    //
    if (IsEqualGUID(&GUID_ACDC_POWER_SOURCE, SettingGuid))
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_POWER,
            "Power State Change Notification");

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

        DWORD PowerState = *(DWORD*)Value;
        switch (PowerState)
        {
        // On Battery
        case PoAc:
            Trace(
                TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "On Battery Power");

            status = Fts521ChangeChargerConnectedState(
                ControllerContext,
                SpbContext,
                0
            );

            if (!NT_SUCCESS(status))
            {
                Trace(
                    TRACE_LEVEL_ERROR,
                    TRACE_POWER,
                    "Error Changing Charger Connected state - 0x%08lX",
                    status);
                goto exit;
            }
            break;
        // Plugged In
        case PoDc:
        case PoHot:
            Trace(
                TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "On External Power");

            status = Fts521ChangeChargerConnectedState(
                ControllerContext,
                SpbContext,
                1
            );

            if (!NT_SUCCESS(status))
            {
                Trace(
                    TRACE_LEVEL_ERROR,
                    TRACE_POWER,
                    "Error Changing Charger Connected state - 0x%08lX",
                    status);
                goto exit;
            }
            break;
        default:
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_POWER,
                "Unknown power state - 0x%02X",
                PowerState);
        }
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
        DWORD GestureEnabled = 0;

        switch (DisplayState)
        // Please note:
        // 1.This method is not applicable to FingerTipS-Touch521 touch 
        //   chip power management, so do not change its power status.
        // 2.Meanwhile, keeping the power on will increase power consumption, 
        //   which is currently an unavoidable issue that we hope to improve 
        //   in the future
        {
        case 0:
            Trace(
                TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "The Display is Off");

            SetScanMode(ControllerContext->FxDevice, SpbContext, SCAN_MODE_LOW_POWER, 0x00);

            break;
        case 1:
            Trace(
                TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "The Display is On");

            SetScanMode(ControllerContext->FxDevice, SpbContext, SCAN_MODE_ACTIVE, 0x01);

            break;
        case 2:
            Trace(
                TRACE_LEVEL_INFORMATION,
                TRACE_POWER,
                "The Display is Dimmed");

            break;
        default:
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_POWER,
                "Unknown display state - 0x%02X",
                DisplayState);
        }
    }

    exit:
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

    controller = (FTS521_CONTROLLER_CONTEXT*) ControllerContext;

    //
    // Check if we were already on
    //
    if (controller->DevicePowerState == PowerDeviceD0)
    {
        goto exit;
    }

    controller->DevicePowerState = PowerDeviceD0;

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

exit:

    return STATUS_SUCCESS;
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

    controller = (FTS521_CONTROLLER_CONTEXT*) ControllerContext;

    //
    // Interrupts are now disabled but the ISR may still be
    // executing, so grab the controller lock to ensure ISR
    // is finished touching HW and controller state.
    //
    WdfWaitLockAcquire(controller->ControllerLock, NULL);

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


    WdfWaitLockRelease(controller->ControllerLock);

    return STATUS_SUCCESS;
}