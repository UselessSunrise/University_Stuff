#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <aclapi.h>
#include <AccCtrl.h>
#include <string.h>
#include <stringapiset.h>
#include <fstream>
#include <process.h>
#include <conio.h>

using namespace std;

void log(const char*, const char*);
void log(const char*, const char*, const char*);
void logInfo(const char*);
void logStart(const char*, const char*);
void logStart();
void logErr(const char*);
bool compareWithMask(LPWSTR, LPWSTR);
void refreshDirectory(LPWSTR);
void deleteIncorrectFile(LPCTSTR);
void renameIncorrectFile(LPCTSTR, LPCTSTR);

DWORD dwWaitStatus;
HANDLE hChangeHandler;
LPCWSTR lpDirectoryName = L"C:/Test";
bool bCloseThread = false;
const char** arr_S_FileMasks;
LPWSTR* arr_W_FileMasks;
OVERLAPPED ovl = { 0 };
int nCountOfMasks = 1;

unsigned __stdcall WatchingThread(void*);

int main() {

	logStart();

	DWORD dwRes;
	PSID pOldEveryoneSID = NULL, pNewEveryoneSID = NULL, pAdminSID = NULL;
	PACL pNewDACL = NULL;
	PACL pOldDACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	EXPLICIT_ACCESS ea[2];
	SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;

	//Reading input data and filling array of masks to look for
	
	ifstream fInput("C:/Test/template.tbl");

	arr_S_FileMasks = (const char**)malloc(sizeof(int) * nCountOfMasks);
	arr_S_FileMasks[0] = "C:/Test/Test.txt";
	arr_W_FileMasks = (LPWSTR*)malloc(sizeof(LPWSTR) * nCountOfMasks);
	arr_W_FileMasks[0] = (LPWSTR)malloc(sizeof(wchar_t) * strlen(arr_S_FileMasks[0]));
	mbsrtowcs(arr_W_FileMasks[0], &(arr_S_FileMasks[0]), strlen(arr_S_FileMasks[0]) + 1, NULL);

	logInfo("Read input data.");

	//Getting old security information to store it

	ULONG lErr = GetNamedSecurityInfo(arr_W_FileMasks[0], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
		&pOldEveryoneSID, NULL, &pOldDACL, NULL, &pSD);
	if (lErr != ERROR_SUCCESS) {
		logErr("Could not get files' security info.");
		return 1;
	}

	logInfo("Got files' current security information.");

	// Create a well-known SID for the Everyone group.
	if (!AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pNewEveryoneSID))
	{
		logErr("Could not allocate and initialize Everyone SID.");
		return 1;
	}

	logInfo("Created new Everyone SID.");

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow Everyone read access to the key.
	ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESS));
	ea[0].grfAccessPermissions = FILE_ALL_ACCESS;
	ea[0].grfAccessMode = DENY_ACCESS;
	ea[0].grfInheritance = NO_INHERITANCE;
	ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea[0].Trustee.ptstrName = (LPTSTR)pNewEveryoneSID;

	// Create a SID for the BUILTIN\Administrators group.
	if (!AllocateAndInitializeSid(&SIDAuthNT, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSID))
	{
		logErr("Could not allocate and initialize Admin SID.");
		return 1;
	}

	logInfo("Created new Admin SID.");

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow the Administrators group full access to
	// the key.

	ea[1].grfAccessPermissions = FILE_ALL_ACCESS;
	ea[1].grfAccessMode = SET_ACCESS;
	ea[1].grfInheritance = NO_INHERITANCE;
	ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
	ea[1].Trustee.ptstrName = (LPTSTR)pAdminSID;

	// Create a new ACL that contains the new ACEs.
	dwRes = SetEntriesInAcl(2, ea, NULL, &pNewDACL);
	if (ERROR_SUCCESS != dwRes)
	{
		logErr("Could not set entries in DACL.");
		return 1;
	}

	//Setting new security attributes
	dwRes = SetNamedSecurityInfo(arr_W_FileMasks[0], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, pNewEveryoneSID, NULL, pNewDACL, NULL);
	if (ERROR_SUCCESS != dwRes)
	{
		logErr("Could not update files security info.");
		return 1;
	}

	logInfo("Set new security information for given files.");

	// Handler of directory to check it for file creating or deleting.

	hChangeHandler = CreateFile(lpDirectoryName,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL
	);

	if (hChangeHandler == INVALID_HANDLE_VALUE) {
		logErr("Could not create directory change handler.");
		ExitProcess(GetLastError());
	}


	// Handler is set and now we are waiting for changes to appear

	logInfo("Created directory change handler.");

	//Creating second thread to watch directory
	unsigned int dwWatchingTreadID;
	HANDLE hWatchingThread = (HANDLE)_beginthreadex(NULL, 0, &WatchingThread, NULL, 0, &dwWatchingTreadID);

	//To exit program user should type password
	Sleep(1000);
	logInfo("Type secret password to stop program.");
	int nPassword;
	while (TRUE) {
		scanf("%d", &nPassword);
		if (nPassword == 1234) {
			break;
		}
		else
			logErr("Incorrect password, try again.");
	}

	//If password was correct we exit the second thread
	logInfo("Exiting watching thread.");
	bCloseThread = true;
	Sleep(1000);
	CloseHandle(hWatchingThread);
	logInfo("Watching thread terminated.");

	//Returning old security attributes to all files
	dwRes = SetNamedSecurityInfo(arr_W_FileMasks[0], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, pOldEveryoneSID, NULL, pOldDACL, NULL);
	if (ERROR_SUCCESS != dwRes)
	{
		logErr("Could not set back file's security info.");
		return 1;
	}

	logInfo("Set back files' security info.");

	//Exiting program
	//Cleaning up all SIDs and descriptors
	if (pOldEveryoneSID)
		FreeSid(pOldEveryoneSID);
	if (pAdminSID)
		FreeSid(pAdminSID);
	if (pNewDACL)
		LocalFree(pNewDACL);
	if (pOldDACL)
		LocalFree(pOldDACL);
	if (pSD)
		LocalFree(pSD);

	return 0;
}

unsigned __stdcall WatchingThread(void* pArguments)
{
	logInfo("Watching thread started.");
	DWORD nBufferLength = 60 * 1024;
	BYTE* lpBuffer = (BYTE*)malloc(sizeof(BYTE) * nBufferLength);
	OVERLAPPED ovl = {};
	DWORD returnedBytes = 0;
	ovl.hEvent = CreateEvent(0, FALSE, FALSE, 0);
	int nLastAction = 0;
	LPWSTR lpOldFileName = (LPWSTR)L" ";


	while (TRUE)
	{
		//Waiting for signal.
		if (bCloseThread)
			break;
		ReadDirectoryChangesW(hChangeHandler, lpBuffer, nBufferLength, TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME, &returnedBytes, &ovl, 0);

		dwWaitStatus = WaitForSingleObject(ovl.hEvent, INFINITE);

		if (dwWaitStatus == WAIT_OBJECT_0) {

			// File was created or deleted.
			// Refresh directory and continue waiting.

			DWORD seek = 0;
			while (seek < nBufferLength)
			{
				PFILE_NOTIFY_INFORMATION pNotify = PFILE_NOTIFY_INFORMATION(lpBuffer + seek);
				seek += pNotify->NextEntryOffset;
				if (pNotify->Action == FILE_ACTION_RENAMED_OLD_NAME || pNotify->Action == FILE_ACTION_ADDED){
					nLastAction = pNotify->Action;
					lpOldFileName = pNotify->FileName;
				}	
				else if (pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME) {
					for (int i = 0; i < nCountOfMasks; i++) {
						if (compareWithMask(pNotify->FileName, arr_W_FileMasks[i])) {
							logInfo("This file name is prohibited. Try using another one.");
							switch (nLastAction) {
							case FILE_ACTION_ADDED: {
								LPWSTR lpFullFileName = (LPWSTR)lpDirectoryName;
								deleteIncorrectFile(wcscat(lpFullFileName, pNotify->FileName));
								break;
							}

							case FILE_ACTION_RENAMED_OLD_NAME: {
								LPWSTR lpOldFullFileName = (LPWSTR)lpDirectoryName, lpNewFullFileName = (LPWSTR)lpDirectoryName;
								renameIncorrectFile(wcscat(lpOldFullFileName, lpOldFileName), wcscat(lpNewFullFileName, pNotify->FileName));
								break;
							}
							}
							break;
						}
					}
					nLastAction = 0;
				}
				if (pNotify->NextEntryOffset == 0) {
					break;
				}
			}
		}
	}

	_endthreadex(0);
	return 0;
}

void refreshDirectory(LPWSTR lpFileName) {
	//If file name matches with one of the masks we return
	//this file it's previous name.
}

void logStart(const char* arg_1, const char* arg_2) {
	log(" INFO Starting programm with following arguments ", arg_1, arg_2);
}

void logStart() {
	log(" START ", "Starting programm without arguments.");
}

void logInfo(const char* msg) {
	log(" INFO ", msg);
}

void logErr(const char* msg) {
	log(" ERROR ", msg);
}

void log(const char* level, const char* msg) {
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("%d-%02d-%02d %02d:%02d:%02d.%03d",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		st.wMilliseconds);
	printf(level);
	printf("%s\n", msg);
}

void log(const char* level, const char* arg_1, const char* arg_2) {
	SYSTEMTIME st;

	GetLocalTime(&st);
	printf("%d-%02d-%02d %02d:%02d:%02d.%03d",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		st.wMilliseconds);
	printf(level);
	printf("%s %s\n", arg_1, arg_2);
}

bool compareWithMask(LPWSTR sLineToCheck, LPWSTR sPattern)
{
	LPWSTR sReturnInLine = 0, sReturnInPattern = 0;
	while (TRUE)
		if (*sPattern == L'*') {
			sReturnInLine = sLineToCheck;
			sReturnInPattern = ++sPattern;
		}
		else if (!*sLineToCheck) {
			return !*sPattern;
		}
		else if (*sLineToCheck == *sPattern || *sPattern == L'?') {
			++sLineToCheck;
			++sPattern;
		}
		else if (sReturnInLine) {
			sLineToCheck = ++sReturnInLine;
			sPattern = sReturnInPattern;
		}
		else
			return false;
}

void deleteIncorrectFile(LPCTSTR lpFileName) {
	DeleteFile(lpFileName);
	return;
}

void renameIncorrectFile(LPCTSTR lpOldFileName, LPCTSTR lpIncorrectFileName) {
	MoveFile(lpIncorrectFileName, lpOldFileName);
	return;
}