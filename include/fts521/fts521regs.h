/*++
	Module Name:

		fts521regs.h

	Abstract:

		Contains common types and defintions used internally
		by the multi touch screen driver.

	Environment:

		Kernel mode

--*/

#define FIFO_EVENT_SIZE					8

#define FTS_CMD_LOCKDOWN_ID				0x70

#define EVT_ID_NOEVENT					0x00
#define EVT_ID_CONTROLLER_READY			0x03
#define EVT_ID_ENTER_POINT				0x13
#define EVT_ID_MOTION_POINT				0x23
#define EVT_ID_LEAVE_POINT				0x33
#define EVT_ID_STATUS_UPDATE			0x43
#define EVT_ID_USER_REPORT				0x53
#define EVT_ID_DEBUG					0xE3
#define EVT_ID_ERROR					0xF3

#define EVT_TYPE_ERROR_ESD				0xF0
#define EVT_TYPE_ERROR_WATCHDOG			0x06	

#define FTS_CMD_SCAN_MODE				0xA0
#define SCAN_MODE_ACTIVE				0x00
#define SCAN_MODE_LOW_POWER				0x01

//
// System command constants (matches Android fts_lib/ftsSoftware.h)
//
#define FTS_CMD_SYSTEM					0xA4
#define SYS_CMD_LOAD_DATA				0x06
#define LOAD_SYS_INFO					0x01

//
// Framebuffer read command and address (matches Android fts_lib/ftsHardware.h)
// For I2C interface the first byte returned from a framebuffer read is a dummy byte.
//
#define FTS_CMD_FRAMEBUFFER_R			0xA6
#define ADDR_FRAMEBUFFER_HIGH			0x00
#define ADDR_FRAMEBUFFER_LOW			0x00
#define HEADER_SIGNATURE				0xA5

//
// System info layout (offsets within the framebuffer read result,
// after skipping the leading dummy byte; matches Android fts_lib/ftsCore.h).
//
#define SYS_INFO_SIZE					208
#define SYS_INFO_BUF_SIZE				(SYS_INFO_SIZE + 1)  /* +1 for I2C dummy byte */
#define SYS_INFO_FW_VER_OFFSET			17                   /* dummy(1) + data_offset(16) */
#define SYS_INFO_CFG_VER_OFFSET			21                   /* dummy(1) + data_offset(20) */

//
// Maximum number of remaining FIFO events that fit in the 256-byte EventBuffer
// after the first event (FIFO_EVENT_SIZE bytes) has already been read.
//
#define FIFO_MAX_REMAIN_EVENTS			((256 / FIFO_EVENT_SIZE) - 1)
