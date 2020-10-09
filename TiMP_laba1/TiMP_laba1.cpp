#define _CRT_SECURE_NO_WARNINGS
#define CATCH_CONFIG_MAIN

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
#include <vector>
#include <string>

using namespace std;

void log(const char*, const char*);
void log(const char*, const char*, const char*);
void logInfo(const char*);
void logStart(const char*, const char*);
void logStart();
void logErr(const char*);
void refreshDirectory(LPWSTR);
void deleteIncorrectFile(LPCTSTR);
void renameIncorrectFile(LPCTSTR, LPCTSTR);



LPCWSTR lpDirectoryName = L"C:/Test";
bool bCloseThread = false;
vector <string> vsFileMasks;
LPWSTR* arrwFileMasks;
OVERLAPPED ovl = { 0 };
const int nMaxBuffSize = 100;

void WatchingThread(void*);

int main() {

	logStart();
	//MoveFile(L"C:/Test/result.txt", L"C:/Test/logs.txt");

	DWORD dwRes;
	PSID pOldEveryoneSID = NULL, pNewEveryoneSID = NULL, pAdminSID = NULL;
	PACL pNewDACL = NULL;
	PACL* arrpOldDACL = (PACL*)malloc(sizeof(PACL) * vsFileMasks.size());
	PSECURITY_DESCRIPTOR pSD = NULL;
	EXPLICIT_ACCESS ea[2];//Structure that sets rights for given SID
	SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY; //SID with everyone group rights
	SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;//SID with Admin group rights

	//Reading input data and filling array of masks to look for

	ifstream fInput("C:/Test/template.tbl");

	if (!fInput.is_open()) {
		logErr("Could not open file with templates.");
		return 1;
	}

	logInfo("Opened file with templates");
	//Skiping first line with encrypted password
	string sNextLine = "";
	getline(fInput, sNextLine);

	while (!fInput.eof()) {
		fInput >> sNextLine;
		string sFullName = "C:/Test/";
		sFullName += sNextLine;
		vsFileMasks.push_back(sFullName.data()); //Reading templates from template.tbl
	}

	fInput.close();

	logInfo("File with templates closed");
	arrwFileMasks = (LPWSTR*)malloc(sizeof(LPWSTR) * vsFileMasks.size());

	for (unsigned int i = 0; i < vsFileMasks.size(); i++) {
		const char* sNextMask = vsFileMasks[i].data();
		arrwFileMasks[i] = (LPWSTR)malloc(sizeof(wchar_t) * strlen(sNextMask));
		mbsrtowcs(arrwFileMasks[i], &(sNextMask), strlen(sNextMask) + 1, NULL); //Transform common string into wide character string
	}

	logInfo("Successfully read and transformed templates.");

	//Getting old security information to store it

	for (unsigned int i = 0; i < vsFileMasks.size(); i++) {
		arrpOldDACL[i] = NULL;
		ULONG lErr = GetNamedSecurityInfo(arrwFileMasks[i], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
			&pOldEveryoneSID, NULL, &arrpOldDACL[i], NULL, &pSD);
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
	for (unsigned int i = 0; i < vsFileMasks.size(); i++) {
		dwRes = SetNamedSecurityInfo(arrwFileMasks[i], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, pNewEveryoneSID, NULL, pNewDACL, NULL);
	}
	logInfo("Set new security information for given files.");

	// Handler of directory to check it for file creating or deleting.


	// Handler is set and now we are waiting for changes to appear

	logInfo("Created directory change handler.");

	//Creating second thread to watch directory
	HANDLE hWatchingThread = (HANDLE)_beginthread(WatchingThread, 1024*2048*sizeof(BYTE) , NULL);

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
	for (unsigned int i = 0; i < vsFileMasks.size(); i++) {
		dwRes = SetNamedSecurityInfo(arrwFileMasks[i], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, pOldEveryoneSID, NULL, arrpOldDACL[i], NULL);
	}

	logInfo("Set back files' security info.");

	//Exiting program
	//Cleaning up all SIDs and descriptors
	if (pOldEveryoneSID)
		FreeSid(pOldEveryoneSID);
	logInfo("Cleaned Everyone SID.");
	if (pAdminSID)
		FreeSid(pAdminSID);
	logInfo("Cleaned Admin SID.");
	if (pNewDACL)
		LocalFree(pNewDACL);
	logInfo("Cleaned pointer to new DACL.");
	for (unsigned int i = 0; i < vsFileMasks.size(); i++)
		if (arrpOldDACL[i])
			LocalFree(arrpOldDACL[i]);
	logInfo("Cleadned pointers to old DACLs.");
	if (pSD)
		LocalFree(pSD);
	logInfo("Cleaned security descriptor.");

	return 0;
}

void WatchingThread(void* pArguments)
{
	logInfo("Watching thread started.");
	//Important data to read directory changes
	int nLastAction = 0, nNameLength = 0;
	bool bRenameFile = FALSE, bDeleteFile = FALSE;

	LPWSTR lpFullFileName = new wchar_t[nMaxBuffSize]; //For deleting/renaming case
	LPWSTR lpOldFullFileName = new wchar_t[nMaxBuffSize]; // For renaming case
	LPWSTR lpOldFileName = new wchar_t[nMaxBuffSize]; // For renaming case
	//MoveFile(L"C:/Test/logs.txt", L"C:/Test/result.txt");
	DWORD nBufferLength = 60 * 1024;
	BYTE* lpBuffer = new BYTE[nBufferLength];
	OVERLAPPED ovl = {}; //Structure for async operations
	ovl.hEvent = CreateEvent(0, FALSE, FALSE, 0); //Event for OVERLAPPED

	while (TRUE)
	{
		//Waiting for signal.
		if (bCloseThread)
			break;

		if (bRenameFile) {
			MoveFile(lpFullFileName, lpOldFullFileName);
			bRenameFile = FALSE;
		}
		if (bDeleteFile) {
			DeleteFile(lpFullFileName);
			bDeleteFile = FALSE;
		}

		HANDLE hChangeHandler = CreateFile(lpDirectoryName,
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

		DWORD returnedBytes = 0;
		ReadDirectoryChangesW(hChangeHandler, lpBuffer, nBufferLength, TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
			&returnedBytes, &ovl, 0);

		DWORD dwWaitStatus = WaitForSingleObject(ovl.hEvent, INFINITE); //Waiting for changes to happen

		if (dwWaitStatus == WAIT_OBJECT_0) {
			DWORD seek = 0;
			while (seek < nBufferLength)
			{
				//Getting info about changes
				PFILE_NOTIFY_INFORMATION pNotify = PFILE_NOTIFY_INFORMATION(lpBuffer + seek);
				seek += pNotify->NextEntryOffset;
				int nAction = pNotify->Action;

				//If file was created or renamed we should check if it's name is correct
				if (nAction == 1) {
					pNotify = PFILE_NOTIFY_INFORMATION(lpBuffer + seek);

					lpOldFileName = pNotify->FileName;
					nNameLength = pNotify->FileNameLength / 2;
					wcscpy(lpFullFileName, lpDirectoryName);
					wcscat(lpFullFileName, L"/");
					wcscat(lpFullFileName, (LPCWSTR)lpOldFileName);
					lpFullFileName[wcslen(lpDirectoryName) + nNameLength + 1] = 0;
					for (unsigned int i = 0; i < vsFileMasks.size(); i++)
						if (wcscmp(lpFullFileName, arrwFileMasks[i]) == 0) {
							bDeleteFile = TRUE;
							break;
						}
				}

				if (nAction == 4) {
					lpOldFileName = pNotify->FileName;
					wcscpy(lpOldFullFileName, lpDirectoryName);
					wcscat(lpOldFullFileName, L"/");
					wcscat(lpOldFullFileName, (LPCWSTR)lpOldFileName);

					pNotify = PFILE_NOTIFY_INFORMATION(lpBuffer + seek);

					lpOldFileName = pNotify->FileName;
					nNameLength = pNotify->FileNameLength / 2;
					wcscpy(lpFullFileName, lpDirectoryName);
					wcscat(lpFullFileName, L"/");
					wcscat(lpFullFileName, (LPCWSTR)lpOldFileName);
					lpFullFileName[wcslen(lpDirectoryName) + nNameLength + 1] = 0;
					for(unsigned int i = 0; i < vsFileMasks.size(); i++)
						if (wcscmp(lpFullFileName, arrwFileMasks[i]) == 0) {
							bRenameFile = TRUE;
							break;
						}
				}
				//If there are no more notes about current changes we begin waiting for the next list of changes
				if (pNotify->NextEntryOffset == 0) {
					break;
				}
			}
		}
		CloseHandle(hChangeHandler);
	}

	CloseHandle(ovl.hEvent);
	delete[] lpBuffer;
	delete lpFullFileName;
	delete lpOldFullFileName;
	delete lpOldFileName;
	//Exiting second thread
	_endthreadex(0);
	return;
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
