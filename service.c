/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: service.c 289 2013-03-07 04:23:41Z kuang $
 *
 *	This file is part of 'ddns', Created by kuang on 2009-09-23.
 *
 *	'ddns' is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published
 *	by the Free Software Foundation; either version 3 of the License,
 *	or (at your option) any later version.
 *
 *	'ddns' is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with 'ddns'; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 *  Interact with NT-Service manager. This file is for Windows only.
 */

#include "service.h"
#include "ddns_sync.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "ddns_string.h"
#include "resource.h"

/**
 *	Re-login after [RESTART_INTERVAL] seconds after keep-alive failure.
 */
#define RESTART_INTERVAL			(10)

#define	DDNS_SERVICE_NAME			_T("Dynamic Domain Name Service")

#define DEFAULT_CONFIGURATION_FILE	"ddns.ini"

/**
 *	Return values of [get_path_type].
 */
#define PATH_INVALID		(-1)
#define PATH_RELATIVE		(0)
#define PATH_NODRIVE		(1)
#define PATH_ABSOLUTE		(2)

/* invalid arguments are specified */
#define ERROR_INVALID_ARGUMENTS			(20001)

/* installing on a non-fixed drive is not allowed */
#define ERROR_NON_LOCAL_EXECUTABLE		(20002)

/* save configuration file on a non-fixed drive is not allowed */
#define ERROR_NON_LOCAL_CONFIGFILE		(20003)

/* path to configuration file is invalid */
#define ERROR_INVALID_CONFIGFILE		(20004)

/* failed to create configuration file sample */
#define ERROR_CREATE_CONFIGFILE			(20005)

/**
 *	Global variables.
 */
static struct ddns_context		*	g_ctx			= NULL;
static SERVICE_STATUS_HANDLE		g_hSvcHandle	= 0;


/**
 *	Main loop, execute DDNS keep-alive/update operation here.
 */
static VOID WINAPI service_main(DWORD dwArgc, LPTSTR *pArgv);


/**
 *	DDNS service control handler. The windows SCM will send control code to this
 *	function to stop/pause/resume/(..., etc.) the service.
 *
 *	@param[in]	dwControl	:	control code, please check the windows platform
 *								SDK for more information.
 */
static VOID WINAPI service_handler(DWORD dwControl);


/**
 *	Update service status in SCM.
 *
 *	@param[in]	dwStatus		: service status.
 *	@param[in]	dwExitCode		: service exit code.
 *	@param[in]	dwCheckPoint	: Specifies a value that the service increments
 *								  periodically to report its progress during a
 *								  lengthy start/stop/pause/continue operation.
 */
static VOID update_service_status(
	DWORD					dwStatus,
	DWORD					dwExitCode,
	DWORD					dwCheckPoint
	);


/**
 *	Test if a path is located on fixed local drivers.
 *
 *	@param[in]	pszPath		: full path to the file/folder.
 *
 *	@note		It does not test if the file/folder actually exists.
 *
 *	@return		Return non-zero if it's a local path, otherwise return 0.
 */
static int is_local_path(const char * pszPath);


/**
 *	Test the type of a path: absolute or relative.
 *
 *	@param[in]	pszPath		: path to the file/folder.
 *
 *	@note		It does not test if the file/folder actually exists.
 *
 *	@return		Type of the path:
 *					- PATH_INVALID	: invalid path
 *					- PATH_RELATIVE	: relative path
 *					- PATH_NODRIVE	: absolute path without driver name
 *					- PATH_ABSOLUTE : absolute path
 */
static int get_path_type(const char * pszPath);


/**
 *	Get absolution path of the executable file & configuration file.
 *
 *	@param[out]	binary		: path to the executable file.
 *	@param[in]	bin_size	: size of the buffer pointed by [binary].
 *	@param[out]	config		: path to the configuration file.
 *	@param[in]	config_size	: size of the buffer pointed by [config].
 *	@param[in]	config_file	: path of configuration file from user.
 *
 *	@return		Return 0 on success, otherwise an error code will be returned.
 *					- ERROR_INVALID_ARGUMENTS
 *					- ERROR_NON_LOCAL_EXECUTABLE
 *					- ERROR_NON_LOCAL_CONFIGFILE
 *					- ERROR_INVALID_CONFIGFILE
 *					- ERROR_CREATE_CONFIGFILE
 */
static size_t get_service_path(
	char		*	binary,
	size_t			bin_size,
	char		*	config,
	size_t			config_size,
	const char	*	config_file
	);


/**
 *	Service table provided by this binary.
 */
static SERVICE_TABLE_ENTRY DDNS_SERTICE_TABLE[] =
{
	{ DDNS_SERVICE_NAME, &service_main },
	{ NULL, NULL }
};


/**
 *	Main entry for starting the DDNS service.
 */
int ddns_nt_service(struct ddns_context* context)
{
	int result = 0;

	g_ctx = context;

	if ( FALSE == StartServiceCtrlDispatcher(DDNS_SERTICE_TABLE) )
	{
		result = 1;
	}

	return result;
}


/**
 *	Install the tool as windows service.
 *
 *	@param[in]	service_name	: name of windows service
 *	@param[in]	config_file		: path to the configuration file
 *	@param[in]	user_name		: user to run the service
 *	@param[in]	password		: password of the user to run the service
 *
 *	@return		Return [ERROR_SUCCESS] on success, otherwise a WIN32 error code
 *				is returned.
 */
int ddns_nt_service_install(
	struct ddns_context	*	context,
	const char			*	service_name,
	const char			*	config_file,
	const char			*	user_name,
	const char			*	password
	)
{
	DWORD		error_code		= ERROR_SUCCESS;
	SC_HANDLE	handle_scm		= NULL;
	SC_HANDLE	handle_svc		= NULL;
	char		binary[1024]	= { 0 };
	char		config[1024]	= { 0 };
	char		command[1024]	= { 0 };

	DDNS_UNUSED(user_name);
	DDNS_UNUSED(password);

	error_code = get_service_path(	binary,
									_countof(binary),
									config,
									_countof(config),
									config_file
									);
	if ( ERROR_SUCCESS == error_code )
	{
		int size = 0;

		size = c99_snprintf(command,	_countof(command),
										"\"%s\" -v --service --config \"%s\"",
										binary,
										config);
		if ( size >= _countof(command) )
		{
			error_code = ERROR_INSUFFICIENT_BUFFER;
		}
	}

	if ( ERROR_SUCCESS == error_code )
	{
		handle_scm = OpenSCManager(	NULL,
									SERVICES_ACTIVE_DATABASE,
									SC_MANAGER_CREATE_SERVICE
									);
		if ( NULL == handle_scm )
		{
			error_code = GetLastError();
		}
	}

	if ( ERROR_SUCCESS == error_code )
	{
		handle_svc = CreateServiceA(handle_scm,
									service_name,
									"Dynamic Domain Name Service",
									GENERIC_READ | GENERIC_WRITE,
									SERVICE_WIN32_OWN_PROCESS,
									SERVICE_AUTO_START,
									SERVICE_ERROR_NORMAL,
									command,
									NULL,
									0,
									"tcpip\0dnscache\0",
									"LocalSystem",
									NULL
									);
		if ( NULL == handle_svc )
		{
			error_code = GetLastError();
		}
	}

	if ( NULL != handle_svc )
	{
		CloseServiceHandle(handle_svc);
		handle_svc = NULL;
	}
	if ( NULL != handle_scm )
	{
		CloseServiceHandle(handle_scm);
		handle_scm = NULL;
	}

	switch( error_code )
	{
	case ERROR_SUCCESS:
		ddns_msg(context,	msg_type_info,
							"Installation successful.\n");
		break;
	case ERROR_NON_LOCAL_EXECUTABLE:
		ddns_msg(context,	msg_type_error,
							"can't install on a network drive or a "
							"removable drive.\n");
		break;
	case ERROR_NON_LOCAL_CONFIGFILE:
		ddns_msg(context,	msg_type_error,
							"configuration can't save configuration "
							"on a network drive or a removable drive.\n");
		break;
	case ERROR_INVALID_CONFIGFILE:
		ddns_msg(context,	msg_type_error,
							"invalid configuration file path is "
							"specified.\n");
		break;
	case ERROR_CREATE_CONFIGFILE:
		ddns_msg(context,	msg_type_error,
							"failed to create configuration file "
							"sample.\n");
		break;
	case ERROR_INVALID_ARGUMENTS:
		ddns_msg(context,	msg_type_error,
							"ERROR: internal error, invalid arguments.\n");
		break;
	case ERROR_SERVICE_EXISTS:
		ddns_msg(context,	msg_type_error,
							"service \"%s\" already exists.\n",
							service_name
							);
		break;
	case ERROR_INVALID_NAME:
		ddns_msg(context,	msg_type_error,
							"invalid service name \"%s\".\n",
							service_name
							);
		break;
	case ERROR_DUPLICATE_SERVICE_NAME:
		ddns_msg(context,	msg_type_error,
							"it's already installed with another "
							"service name.\n");
		break;
	case ERROR_ACCESS_DENIED:
		ddns_msg(context,	msg_type_error,
							"failed to create service, please make "
							"sure you have enough permission.\n");
		break;
	default:
		ddns_msg(context,	msg_type_error,
							"unknown error[%u].\n",
							error_code
							);
		break;
	}

	return error_code;
}


/**
 *	DDNS service control handler. The windows service manager will send control
 *	code to this function to stop/pause/resume/(..., etc.) the service.
 *
 *	@param[in]	dwControl	:	control code, please check the windows platform
 *								SDK for more information.
 */
static VOID WINAPI service_handler(DWORD dwControl)
{
	switch ( dwControl )
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		update_service_status(SERVICE_STOP_PENDING, 0, 0);
		if ( NULL != g_ctx )
		{
			if ( 0 == ddns_sync_lock(&(g_ctx->sync_object)) )
			{
				g_ctx->exit_signal = 1;

				ddns_sync_unlock(&(g_ctx->sync_object));
			}
		}
		break;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}
}


/**
 *	Main loop, execute DDNS keep-alive/update operation here.
 */
static VOID WINAPI service_main(DWORD dwArgc, LPTSTR *pArgv)
{
	DDNS_UNUSED(dwArgc);
	DDNS_UNUSED(pArgv);

	g_hSvcHandle = RegisterServiceCtrlHandler(DDNS_SERVICE_NAME, &service_handler);
	if ( 0 == g_hSvcHandle )
	{
		return;
	}

	update_service_status(SERVICE_RUNNING, 0, 0);

	while ( NULL != g_ctx )
	{
		int signal = 0;

		ddns_error err_code = ddns_execute(g_ctx);
		if (DDNS_IS_FATAL_ERROR(err_code))
		{
			break;
		}

		ddns_sync_lock(&(g_ctx->sync_object));
		signal = g_ctx->exit_signal;
		ddns_sync_unlock(&(g_ctx->sync_object));
		if ( 0 == signal )
		{
			/* retry after several seconds */
			Sleep(RESTART_INTERVAL * 1000);
		}
		else
		{
			break;
		}
	}

	update_service_status(SERVICE_STOPPED, 0, 0);
}


/**
 *	Update service status in SCM.
 *
 *	@param[in]	dwStatus		: service status.
 *	@param[in]	dwExitCode		: service exit code.
 *	@param[in]	dwCheckPoint	: Specifies a value that the service increments
 *								  periodically to report its progress during a
 *								  lengthy start/stop/pause/continue operation.
 */
static VOID update_service_status(
	DWORD					dwStatus,
	DWORD					dwExitCode,
	DWORD					dwCheckPoint )
{
	SERVICE_STATUS			sSvcStatus;

	DDNS_UNUSED(dwExitCode);
	DDNS_UNUSED(dwCheckPoint);

	sSvcStatus.dwServiceType				= SERVICE_WIN32_OWN_PROCESS;
	sSvcStatus.dwCurrentState				= dwStatus;
	sSvcStatus.dwControlsAccepted			= SERVICE_ACCEPT_STOP
											| SERVICE_ACCEPT_SHUTDOWN;
	sSvcStatus.dwWin32ExitCode				= NO_ERROR;
	sSvcStatus.dwServiceSpecificExitCode	= 0;
	sSvcStatus.dwCheckPoint					= 0;
	sSvcStatus.dwWaitHint					= 5 * 1000;

	if ( SERVICE_START_PENDING == dwStatus )
	{
		sSvcStatus.dwControlsAccepted = 0;
	}

	SetServiceStatus(g_hSvcHandle, &sSvcStatus);
}


/**
 *	Test if a path is located on local fixed drivers.
 *
 *	@param[in]	pszPath		: full path to the file/folder.
 *
 *	@note		It does not test if the file/folder actually exists.
 *
 *	@return		Return non-zero if it's a local path, otherwise return 0.
 */
static int is_local_path(const char * pszPath)
{
	int bIsLocal = 0;

	if ( NULL != pszPath )
	{
		bIsLocal = 1;

		if (	(pszPath[0] < 'a' || pszPath[0] > 'z')
			&&	(pszPath[0] < 'A' || pszPath[0] > 'Z') )
		{
			bIsLocal = 0;
		}
		else if ( ('\0' != pszPath[0]) && (':' != pszPath[1]) )
		{
			bIsLocal = 0;
		}
		else
		{
			char szRootDir[4];
			c99_snprintf(szRootDir, _countof(szRootDir), "%.*s", 3, pszPath);
			if ( DRIVE_FIXED != GetDriveTypeA(szRootDir) )
			{
				bIsLocal = 0;
			}
		}
	}

	return bIsLocal;
}


/**
 *	Get absolution path of the executable file & configuration file.
 *
 *	@param[out]	binary		: path to the executable file.
 *	@param[in]	bin_size	: size of the buffer pointed by [binary].
 *	@param[out]	config		: path to the configuration file.
 *	@param[in]	config_size	: size of the buffer pointed by [config].
 *	@param[in]	config_file	: path of configuration file from user.
 *
 *	@return		Return 0 on success, otherwise an error code will be returned.
 *					- ERROR_INVALID_ARGUMENTS
 *					- ERROR_NON_LOCAL_EXECUTABLE
 *					- ERROR_NON_LOCAL_CONFIGFILE
 *					- ERROR_INVALID_CONFIGFILE
 *					- ERROR_CREATE_CONFIGFILE
 */
static size_t get_service_path(
	char		*	binary,
	size_t			bin_size,
	char		*	config,
	size_t			config_size,
	const char	*	config_file
	)
{
	DWORD error_code = ERROR_SUCCESS;

	if (	(NULL == binary) || (0 == bin_size)
		||	(NULL == config) || (0 == config_size) )
	{
		error_code = ERROR_INVALID_ARGUMENTS;
	}

	if ( (NULL == config_file) || ('\0' == config_file[0]) )
	{
		config_file = DEFAULT_CONFIGURATION_FILE;
	}

	/**
	 *	Step 1: Get absolute path of the executable file.
	 */
	if ( ERROR_SUCCESS == error_code )
	{
		DWORD dwLength = 0;

		dwLength = GetModuleFileNameA(NULL, binary, bin_size);
		if ( 0 == dwLength )
		{
			error_code = GetLastError();
		}
		else if ( 0 == is_local_path(binary) )
		{
			error_code = ERROR_NON_LOCAL_EXECUTABLE;
		}
	}

	/**
	 *	Step 2: Get absolute path of configuration file.
	 */
	if ( ERROR_SUCCESS == error_code )
	{
		const char	*	last_chr	= NULL;
		DWORD			dwPathLen	= 0;

		switch ( get_path_type(config_file) )
		{
		case PATH_ABSOLUTE:	/* absolute path */
			dwPathLen = GetFullPathName(config_file, config_size, config, NULL);
			break;

		case PATH_NODRIVE:	/* absolute path without drive name */
			dwPathLen = c99_snprintf(	config,
										config_size,
										"%.*s%s",
										2,
										binary,
										config_file
										);
			if ( dwPathLen >= config_size )
			{
				error_code = ERROR_INSUFFICIENT_BUFFER;
			}
			break;

		case PATH_RELATIVE:	/* relative path */
			last_chr = strrchr(binary, '\\');
			if ( NULL == last_chr )
			{
				error_code = ERROR_INVALID_CONFIGFILE;
			}
			else
			{
				dwPathLen = c99_snprintf(	config,
											config_size,
											"%.*s%s",
											last_chr - binary + 1,
											binary,
											config_file
											);
				if ( dwPathLen >= config_size )
				{
					error_code = ERROR_INSUFFICIENT_BUFFER;
				}
			}
			break;

		case PATH_INVALID:
		default:
			error_code = ERROR_INVALID_CONFIGFILE;
			break;
		}

		if ( (ERROR_SUCCESS == error_code) && (0 == is_local_path(config)) )
		{
			/* only local path is allowed */
			error_code = ERROR_NON_LOCAL_CONFIGFILE;
		}
	}

	/**
	 *	Step 3: make sure configuration file exists
	 */
	if ( ERROR_SUCCESS == error_code )
	{
		FILE * fconfig = fopen(config, "r");
		if ( NULL != fconfig )
		{
			fclose(fconfig);
			fconfig = NULL;
		}
		else
		{
			HRSRC			data_info	= NULL;
			DWORD			data_size	= 0;
			HGLOBAL			data_handle	= NULL;
			const char	*	sample		= NULL;

			data_info = FindResource(NULL,
									MAKEINTRESOURCE(IDR_CONFIG_TEMPLATE),
									_T("TEXT")
									);
			if ( NULL == data_info )
			{
				error_code = ERROR_PATH_NOT_FOUND;
			}
			if ( ERROR_SUCCESS == error_code )
			{
				data_size = SizeofResource(NULL, data_info);
				if ( 0 == data_size )
				{
					error_code = ERROR_PATH_NOT_FOUND;
				}
			}
			if ( ERROR_SUCCESS == error_code )
			{
				data_handle = LoadResource(NULL, data_info);
				if ( NULL == data_handle )
				{
					error_code = ERROR_PATH_NOT_FOUND;
				}
			}
			if ( ERROR_SUCCESS == error_code )
			{
				sample = LockResource(data_handle);
				if ( NULL == sample )
				{
					error_code = ERROR_PATH_NOT_FOUND;
				}
			}
			if ( ERROR_SUCCESS == error_code )
			{
				fconfig = fopen(config, "w");
				if ( NULL != fconfig )
				{
					if ( data_size != fwrite(sample, 1, data_size, fconfig) )
					{
						error_code = ERROR_CREATE_CONFIGFILE;
					}
					fclose(fconfig);

					if ( ERROR_SUCCESS != error_code )
					{
						unlink(config);
					}
				}
				else
				{
					error_code = ERROR_CREATE_CONFIGFILE;
				}
			}
		}
	}

	return error_code;
}


/**
 *	Test the type of a path: absolute or relative.
 *
 *	@param[in]	pszPath		: path to the file/folder.
 *
 *	@note		It does not test if the file/folder actually exists.
 *
 *	@return		Type of the path:
 *					- PATH_INVALID	: invalid path
 *					- PATH_RELATIVE	: relative path
 *					- PATH_NODRIVE	: absolute path without driver name
 *					- PATH_ABSOLUTE : absolute path
 */
static int get_path_type(const char * pszPath)
{
	char			drive_name	= '\0';
	const char	*	file_path	= NULL;
	int				path_type	= PATH_INVALID;

	if ( (NULL == pszPath) || ('\0' == pszPath[0]) )
	{
		return path_type;
	}

	if ( '\\' == pszPath[0] )
	{
		path_type = PATH_NODRIVE;
		file_path = pszPath + 1;
	}
	else if ( ('\0' != pszPath[0]) && (':' == pszPath[1]) )
	{
		drive_name	= pszPath[0];
		if ( '\\' != pszPath[2] )
		{
			/* invalid path */
		}
		else if ( (drive_name >= 'a') && (drive_name <= 'z') )
		{
			int drive_idx = drive_name - 'a';
			if ( 0 != (GetLogicalDrives() & (1 << drive_idx)) )
			{
				path_type = PATH_ABSOLUTE;
				file_path = pszPath + 3;
			}
		}
		else if ( (drive_name >= 'A') && (drive_name <= 'Z') )
		{
			int drive_idx = drive_name - 'A';
			if ( 0 != (GetLogicalDrives() & (1 << drive_idx)) )
			{
				path_type = PATH_ABSOLUTE;
				file_path = pszPath + 3;
			}
		}
		else
		{
			/* invalid disk drive */
		}
	}
	else
	{
		path_type = PATH_RELATIVE;
		file_path = pszPath;
	}

	/* check if [file_path] is a legal path */
	for ( ; '\0' != file_path[0]; ++file_path )
	{
		if ( NULL != strchr(":*?\"<>|", *file_path) )
		{
			path_type = PATH_INVALID;
			break;
		}
	}

	return path_type;
}
