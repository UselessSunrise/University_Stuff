#define _CRT_SECURE_NO_WARNINGS

#include <fstream>
#include <windows.h>
#include <stdio.h>

using namespace std;

void log(const char*, const char*);
void log(const char*, const char*, const char*);
void logInfo(const char*);
void logStart(const char*, const char*);
void logStart();
void logErr(const char*);

const int nMaxUsages = 10; //Limit of max usages
const int nMaxNameLen = 50;

int main()
{
	logStart();
	int nUsagesLeft;

	ifstream fInput("C:/Test/Data.txt");

	if (!fInput.is_open()) {
		//If data file does not exist we need to create one
		fInput.close();
		HANDLE hInput = CreateFile(L"C:/Test/Data.txt",
			GENERIC_ALL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL,
			CREATE_ALWAYS,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL
		);
		if (hInput == INVALID_HANDLE_VALUE) {
			logErr("Could not create data file.");
			ExitProcess(GetLastError());
		}
		CloseHandle(hInput);
		logInfo("Data file created.");

		//Now we need to write max number of usages or system time into it
		ofstream fOutput("C:/Test/Data.txt");
		fOutput << nMaxUsages << endl;
		fOutput.close();
		fInput.open("C:/Test/Data.txt");
	}

	fInput >> nUsagesLeft;
	
	if(nUsagesLeft == 0){
		logInfo("Your trial is finished.");
		logInfo("To continue using this software you can buy full version on our website.");
	}
	else {
		char sNameBuffer[nMaxNameLen];
		logInfo("Hello. Please enter your name below to use this software.");
		scanf("%s", sNameBuffer);
	}
	logInfo("Exiting program.");
	Sleep(10000);
    return 0;
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