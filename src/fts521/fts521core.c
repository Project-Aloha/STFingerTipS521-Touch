/*++
	  Module Name:

			fts521core.c

	  Abstract:

			Contains STFingerTipSTouch Core code

	  Environment:

			Kernel mode

	  Revision History:

--*/

#include <Cross Platform Shim\compat.h>
#include <fts521\fts521core.h>
#include <fts521\fts521regs.h>
#include <fts521core.tmh>

NTSTATUS 
SetScanMode(
	SPB_CONTEXT* SpbContext,
	BYTE Mode,
	BYTE Settings)
{
	NTSTATUS status;

	// FTS521_SCAN_MODE: 
	// * Address: FTS_CMD_SCAN_MODE
	// * { FTS_CMD_SCAN_MODE, Mode, Settings };
	BYTE FTS521_SCAN_MODE[2] = { Mode, Settings };
	status = SpbWriteDataSynchronously(SpbContext, FTS_CMD_SCAN_MODE, FTS521_SCAN_MODE, sizeof(FTS521_SCAN_MODE));

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