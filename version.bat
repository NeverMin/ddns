@echo off

REM
REM  Copyright (C) 2009-2010 K.R.F. Studio.
REM
REM  $Id: version.bat 286 2013-02-16 03:04:11Z kuang $
REM
REM  This file is part of 'ddns', Created by karl on 2009-05-30.
REM
REM  'ddns' is free software; you can redistribute it and/or modify
REM  it under the terms of the GNU General Public License as published
REM  by the Free Software Foundation; either version 3 of the License,
REM  or (at your option) any later version.
REM
REM  'ddns' is distributed in the hope that it will be useful, but
REM  WITHOUT ANY WARRANTY; without even the implied warranty of
REM  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM  GNU General Public License for more details.
REM
REM  You should have received a copy of the GNU General Public License
REM  along with 'ddns'; if not, write to the Free Software Foundation, Inc.,
REM  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. 
REM

setlocal ENABLEDELAYEDEXPANSION

set DDNS_VER=
set SVN_FOLDER=
set SVN_REVISION=
set DEBUG=off

if exist .svn (
	set SVN_FOLDER=.svn
) else (
	if exist _svn (
		set SVN_FOLDER=_svn
	)
)

rem
rem Step 1: use "svnversion" to get the source version.
rem
call :GET_PATH svnversion "%PATH%;C:\Program Files\Subversion\bin;C:\Program Files\CollabNet Subversion;C:\Program Files\CollabNet Subversion Client;C:\Program Files\TortoiseSVN\bin"
if NOT ERRORLEVEL 1 (
	if "on" == "%DEBUG%" (
		echo Tool found at !_OUT!
	)
	(
		for /F %%i in ('!_OUT!') do (
			set SVN_REVISION=%%i
		)
	) 2>nul
) else (
	if "on" == "%DEBUG%" (
		echo Tool "svnversion" not found.
	)
)

rem
rem Step 2: use "svn info" to get the source version.
rem
if "%SVN_REVISION%" == "" (
	call :GET_PATH svn "%PATH%;C:\Program Files\Subversion\bin;C:\Program Files\CollabNet Subversion;C:\Program Files\CollabNet Subversion Client;C:\Program Files\TortoiseSVN\bin"
	if NOT ERRORLEVEL 1 (
		if "on" == "%DEBUG%" (
			echo Tool found at !_OUT!
		)
		(
			for /F "delims=: tokens=1,2" %%i in ('!_OUT! info') do (
				if "Revision" == "%%i" (
					for %%k in (%%j) do (
						set SVN_REVISION=%%k
					)
				)
			)
		) 2>nul
		if "!SVN_REVISION!" NEQ "" (
			for /F "tokens=1,2" %%i in ('!_OUT! stat -q') do (
				set _MODIFIED=1
			)
		)
		if "!_MODIFIED!" == "1" (
			set SVN_REVISION=!SVN_REVISION!M
		)
	) else (
		if "on" == "%DEBUG%" (
			echo Tool "svn" not found.
		)
	)
)

rem
rem Step 3: Get directory revision if all the above fail.
rem
if "%SVN_REVISION%" == "" (
	if exist %SVN_FOLDER%\entries (
		for /F "usebackq" %%i in ("%SVN_FOLDER%\entries") do (
			if "dir" == "!SVN_REVISION!" (
				set SVN_REVISION=%%i
			) else (
				if "%%i" == "dir" (
					if "" == "!SVN_REVISION!" (
						set SVN_REVISION=dir
					)
				)
			)
		)
	)
)

rem
rem Step 4: Output source revision and create file "ddns_version.h".
rem
if exist ddns_version.h (
	for /F "tokens=1,2,3" %%i in (ddns_version.h) do (
		if NOT "%%i" == "#define" (
			exit /b 1
		)
		if NOT "%%j" == "REPOSITORY_REVISION" (
			exit /b 1
		)
		if NOT "%%~k" == "%SVN_REVISION%" (
			set DDNS_VER=#define REPOSITORY_REVISION "%SVN_REVISION%"
		)
	)
) else (
	set DDNS_VER=#define REPOSITORY_REVISION "%SVN_REVISION%"
)
if NOT "" == "%DDNS_VER%" (
	echo Updating source revision...
	echo %DDNS_VER% 1> ddns_version.h 2>nul
)

exit /b 0


:GET_PATH
call :_GET_PATH "%~1" ".;%~2"
if ERRORLEVEL 1 (
	call :_GET_PATH "%~1" "%PATH%"
)
goto :EOF

:_GET_PATH
for /F "delims=; tokens=1,*" %%i in ("%~2") do (
	call :_GET_PATH_EXT "%~1" "%PATHEXT%" "%%~i"
	if NOT ERRORLEVEL 1 (
		exit /b 0
	) else (
		if NOT "%%~j" == "" (
			call :_GET_PATH "%~1" "%%~j"
			if NOT ERRORLEVEL 1 (
				exit /b 0
			)
		)
	)
)
exit /b 1

:_GET_PATH_EXT
for /F "delims=; tokens=1,*" %%i in ("%~2") do (
	if exist "%~f3\%~1%%i" (
		set _OUT="%~f3\%~1%%i"
		exit /b 0
	) else (
		if NOT "" == "%%j" (
			call :_GET_PATH_EXT "%~1" "%%~j" "%~3"
			if NOT ERRORLEVEL 1 (
				exit /b 0
			)
		)
	)
)
exit /b 1
