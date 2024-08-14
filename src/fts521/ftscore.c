/*++
      Module Name:

            ftscore.c

      Abstract:

            Contains FingerTipS521 Core code

      Environment:

            Kernel mode

      Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <fts521\ftsinternal.h>
#include <ftscore.tmh>

NTSTATUS 
SetScanMode(
    WDFDEVICE Device,
    SPB_CONTEXT* SpbContext,
    BYTE Mode,
    BYTE Settings)
{
    NTSTATUS status;
    BYTE FTS521_SCAN_MODE[3] = { FTS_CMD_SCAN_MODE, Mode, Settings };

    status = FtsWrite(SpbContext, FTS521_SCAN_MODE, 3);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_INTERRUPT,
            "Setting scan mode Error");
    }

    Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_INTERRUPT,
            "Setting scan mode OK");

    return STATUS_SUCCESS;
}