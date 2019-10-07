// LALUsb.h : main header file of LALusbv2 project
// (LALUsb 2.0  V 1.0.2.1)
// Author C. CHEIKALI
// Created 06/06/07
// Last modified :
//		17/02/2011 : Added Synchronous i/o mode management for 2232H
//		11/01/2011 : two error codes added 
//		26/05/2010 : Interrupt and frame management routines are now reachable from the DLL
//		31/03/2008 : Adapted to the latest version of ftd2xx.h

#ifndef __USB_LAL_H__
#define __USB_LAL_H__

#ifdef _WINDOWS_

	#ifdef LALUSB_EXPORTS
		#define LALUSB_API __declspec(dllexport)
		#define _cstmcall	_stdcall
	#else
		#ifdef LALUSB_STATIC
			#define LALUSB_API
			#define _cstmcall	__cdecl
		#else
			#define LALUSB_API __declspec(dllimport)
			#define _cstmcall	_stdcall
		#endif
	#endif
#else
	#define LALUSB_API
	#define _cstmcall

	#include <fcntl.h>
	#include <sys/types.h>
	#include <sys/stat.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftd2xx.h"

/******************************* definitions ******************************/

typedef enum
{
	RXSTATUS,
	TXSTATUS
}TXRX_STATUS;

#define MAXDEVS								40	// This number may be increased up to 127

// Printf-like function type

#if !defined (PrintfFunc)
	typedef void (*PrintfFunc)(char *format, ...);
#endif

// Error printing function type

#if !defined (PerrorFunc)
	typedef void (*PerrorFunc)(char *msg);
#endif

// Errors that may occur

#define USB_NoErr	FT_OK
#define FrameNoErr	FT_OK

typedef FT_STATUS USB_Error;

#ifndef LALUSB_EXPORTS
	extern LALUSB_API PerrorFunc pfError;
#endif

/******************************* Prototypes ******************************/

#ifdef __cplusplus
extern "C" {
#endif

// Initialization

LALUSB_API BOOL	_cstmcall	USB_FindDevices			(char *DeviceDescriptionStr);
LALUSB_API BOOL	_cstmcall	USB_Init				(int id, BOOL verbose);
LALUSB_API BOOL	_cstmcall	USB_ResetDevice			(int id);
LALUSB_API FT_STATUS _cstmcall	GetDeviceSerNum			(char *buffer, int index);
LALUSB_API FT_STATUS _cstmcall	GetDeviceDesc			(char *buffer, int index);
LALUSB_API int	_cstmcall	USB_GetNumberOfDevs		(void);

// Open and close devices

LALUSB_API int	_cstmcall	OpenUsbDevice			(char *sernumstr);
LALUSB_API void	_cstmcall	CloseUsbDevice			(int id);
LALUSB_API void	_cstmcall	USB_CloseAll			(void);

LALUSB_API void	_cstmcall	dbg_OpenLogFile			(void);
LALUSB_API void	_cstmcall	dbg_CloseLogFile		(void);

// Buffers and I/O operation

LALUSB_API BOOL	_cstmcall	USB_PurgeBuffers		(int id);
LALUSB_API BOOL	_cstmcall	USB_Write				(int id, void *buf, int count, int *written);
LALUSB_API BOOL	_cstmcall	USB_Read				(int id, void *buf, int maxcnt, int *rdcount);
LALUSB_API int	_cstmcall	USB_GetStatus			(int id, TXRX_STATUS);
// LALUSB_API int			USB_GetStatusEx			(int id, TXRX_STATUS stat, DWORD *pEventDWord);

// Parameters

LALUSB_API BOOL	_cstmcall	USB_SetTimeouts			(int id, int tx_timeout, int rx_timeout);
LALUSB_API BOOL	_cstmcall	USB_SetXferSize			(int id, unsigned long txsize, unsigned long rxsize);
LALUSB_API BOOL	_cstmcall	USB_SetLatencyTimer		(int id, UCHAR msecs);
LALUSB_API BOOL	_cstmcall	USB_GetLatencyTimer		(int id, PUCHAR msecs);
LALUSB_API BOOL _cstmcall	USB_SetBaudRate			(int id, int baud);

// Error handling

LALUSB_API void	_cstmcall	USB_Perror				(USB_Error err_code);
LALUSB_API USB_Error	_cstmcall USB_GetLastError	(void);
LALUSB_API void	_cstmcall	USB_SetPerrorFunc		(PerrorFunc func);
LALUSB_API void	_cstmcall	USB_PrintErrMsg			(char *msg);
LALUSB_API char * _cstmcall	GetErrMsg				(USB_Error err_code);

void						USB_Printf				(char *format,...);
LALUSB_API void _cstmcall	USB_SetPrintfFunc		(PrintfFunc func);
USB_Error					GetUSB_Errno			(void);

// IO transfer mode management (asynchronous or synchronous)

LALUSB_API BOOL	_cstmcall	USB_ResetMode(int id);   						// Tranfer mode is set to asynchronous i/o
LALUSB_API BOOL	_cstmcall	USB_SetSynchronousMode(int id, int sleep_time);	// sleep_time is the time (in ms) between resetting tranfer mode and setting it to synchronous
LALUSB_API BOOL	_cstmcall	USB_SetFlowControlToHardware(int id);

// EEPROM management

LALUSB_API BOOL	_cstmcall	USB_EraseEEpromData		(int id);
LALUSB_API BOOL	_cstmcall	USB_WriteEEpromData		(int id);
LALUSB_API BOOL	_cstmcall	USB_ReadEEpromData		(int id, BOOL verbose);
LALUSB_API BOOL	_cstmcall	USB_SetSerialNumber		(int id, char *sernum);
LALUSB_API char	* _cstmcall  USB_GetDefaultSerialNum(void);
LALUSB_API char	* _cstmcall	USB_GetEpromSerialNum	(void);
LALUSB_API BOOL	_cstmcall	USB_SetDescStr			(int id, char *desc_str);
LALUSB_API char	* _cstmcall	USB_GetDefaultDeviceDesc(void);
LALUSB_API char	* _cstmcall	USB_GetEpromDeviceDesc	(void);
LALUSB_API BOOL	_cstmcall	USB_SetPowerSource		(int id, unsigned short powersrc);
LALUSB_API BOOL	_cstmcall	USB_SetMaxPower			(int id, unsigned short current);

#ifdef __cplusplus
}
#endif

// ********************************************************************************
// ********************************************************************************
// **************************** LAL Frames Management *****************************
// ********************************************************************************
// ********************************************************************************

// Definitions

#define FRAME_HEADER	0x000000AA
#define FRAME_TRAILER	0x00000055
#define IS_INTERRUPT	0x0000007F

#define DATA_READ		0x00000080
#define DATA_WRITE		0x0000007F

#define USER_IRQ_FLAG	0x00000080

/*
#ifndef _WINDOWS_
	typedef unsigned char UCHAR;
	
	#define BOOL	unsigned long
	#ifndef FALSE
		#define FALSE			0 
		#define TRUE			!FALSE
	#endif
#endif
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _frame_error_constants
{
	FE_NONE,						// No frame error nor interrupt
	FE_DEVICE,						// Error (Interrupt) came from device
	FE_SOFTWARE,					// Error detected by the software
	FE_USER,						// User interrupt
	FE_ERROR,						// Interrupt has been generated by an error
	FE_HEADER,						// Header error
	FE_TRAILER,						// Trailer error
	MaxFrameError
}FrameError;
// USB_IO_ERROR

typedef enum _extended_usb_errors
{
	USB_INT_FRAME = FT_OTHER_ERROR + 2,
	IO_TIMEOUT_WRITE,
	IO_TIMEOUT_READ,
	BAD_HEADER_ERROR,
	BAD_TRAILER_ERROR,
	BAD_SUBADDR_ERROR,
	NULL_DESCSTR_PTR,
	NULL_SERNUM_PTR,
	Max_USB_Errors
}Ext_USB_Error;

typedef struct _int_frame_info
{
	int source;					// interrupt source : device (interrupt) or software (error)
	int type;					// interrupt type : error or user interrupt
	int error_type;				// error type : header, trailer or none if it's a user interrupt
	int io_dir;					// specifies the type of io operation (in/out)
	int sub_addr;				// current sub-address or command
	int last_sub_addr;			// last command for which data were processed normally
	int status;					// incoming user-defined status bits
	USB_Error err_code;			// FTDI error_code if the error comes from the chip/driver
}InterruptFrameInfo, *InterruptFrameInfoPtr;

typedef enum _fake_errors
{
	FK_LETSBETRUE = 1,			// No error
	FK_HEADER = 2,				// header
	FK_TRAILER = 4,				// trailer
	FK_COUNT = 8,				// bad number of bytes
	MaxFakeErrors = 16
}FakeErrorType;

// Prototypes

LALUSB_API	int		_cstmcall	UsbWrt						(int id, char sub_addr, void *buffer, int count);
LALUSB_API	int		_cstmcall	UsbRd						(int id, char sub_addr, void *array, int count);
LALUSB_API	int		_cstmcall	UsbRdEx						(int id, void *array, int maxcnt, int *frames);
LALUSB_API	int		_cstmcall	UsbReadEx					(int id, void *array, int maxcnt, int *frames, int *frame_size);

LALUSB_API	void	_cstmcall	UsbSetIntCheckingState		(BOOL truefalse);
LALUSB_API	void	_cstmcall	PrintFrameInfo				(void);
LALUSB_API	int		_cstmcall	GetLastFrameStatus			(void);
LALUSB_API	InterruptFrameInfoPtr _cstmcall		GetLastFrame(void);
LALUSB_API	void	_cstmcall	ResetLostFrames				(void);
LALUSB_API	void	_cstmcall	ResetFrameErrors			(void);
LALUSB_API	ULONG	_cstmcall	GetLostFrames				(void);
LALUSB_API	ULONG	_cstmcall	GetTotalByteCount			(void);
LALUSB_API	void	_cstmcall	ResetTotalByteCount			(void);

LALUSB_API	ULONG	_cstmcall	GetHeaderErrors				(void);
LALUSB_API	ULONG	_cstmcall	GetTrailerErrors			(void);

LALUSB_API	void	_cstmcall	GenerateFakeError			(FakeErrorType fet, BOOL onoff);

#ifdef __cplusplus
}
#endif

#endif // __USB_LAL_H__

