#include "pch.h"
#include "cmake_server.h"
#include <cstdio>
#include <tchar.h>
#include <strsafe.h>

HANDLE g_hChildStd_IN_Rd = nullptr;
HANDLE g_hChildStd_IN_Wr = nullptr;
HANDLE g_hChildStd_OUT_Rd = nullptr;
HANDLE g_hChildStd_OUT_Wr = nullptr;

HANDLE g_hInputFile = nullptr;

void CreateChildProcess(void);
void WriteToPipe(void);
void ReadFromPipe(void);
void ErrorExit(const char*);

// see: https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-a-child-process-with-redirected-input-and-output
int entry_point(int argc, char* argv[])
{
	SECURITY_ATTRIBUTES saAttr;

	// Set the bInheritHandle flag so pipe handles are inherited
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = nullptr;

	// Create a pipe for the child process's stdout
	if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
		ErrorExit(TEXT("StdoutRd CreatePipe"));

	// Ensure the read handle to the pipe for STDOUT is not inherited
	if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
		ErrorExit(TEXT("Stdout SetHandleInformation"));

	// Create a pipe for the child process's STDIN
	if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
		ErrorExit(TEXT("Stdin CreatePipe"));

	// Ensure the write handle to the pipe for STDIN is not inherited
	if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
		ErrorExit(TEXT("Stdin SetHandleInformation"));

	// Create the child process
	CreateChildProcess();

	// Get a handle to an input file for the parent
	// This example assumes a plain text file and uses string output to verify data flow
	if (argc == 1)
		ErrorExit(TEXT("Please specify an input file.\n"));

	g_hInputFile = CreateFile(
		argv[1],
		GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_READONLY,
		nullptr
	);

	if (g_hInputFile == INVALID_HANDLE_VALUE)
		ErrorExit(TEXT("CreateFile"));

	// Write to the pipe that is the standard input for a child process
	// Data is written to the pipe's buffers, so it is no necessary to wait
	// unit the child process is running before writing the data
	WriteToPipe();
	printf("\n->Contents of %s written to child STDIN pipe.\n", argv[1]);
	
	// Flush the buffer incase it wasnt flushed for whatever reason
	FlushFileBuffers(g_hChildStd_IN_Wr);
	
	// Read from pipe that is the standard output for child process
	printf("\n->Contents of child process STDOUT:%s\n\n", argv[1]);
	ReadFromPipe();

	printf("\n->End of parent execution");

	// The remaining open handles are cleaned up when this process terminates
	// To avoid resource leaks in a larger application close handles here
	return 0;
}

void CreateChildProcess(void)
{
	// Create a child process that uses the previously create pipes for STDIN and STDOUT
	TCHAR szCmdline[] = TEXT("cmake -E server --experimental --debug");
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;

	// setup members of the PROCESS_INFOMATION struct
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// setup memebers of the STARTUPINFO struct
	// specifies stdout and stdin
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process
	bSuccess = CreateProcess(nullptr,
		szCmdline,
		nullptr,
		nullptr,
		TRUE,
		0,
		nullptr,
		nullptr,
		&siStartInfo,
		&piProcInfo);

	if (!bSuccess)
		ErrorExit(TEXT("CreateProcess"));
	else
	{
		// Close handles to the child process and its primary thread.
		// Some applications might keep these handles to monitor the status
		// of the child process, for example. 
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
	}
}

void WriteToPipe(void)
{
	DWORD dwRead, dwWritten;
	CHAR chBuf[BUFSIZ];
	BOOL bSuccess = FALSE;

	for (;;)
	{
		bSuccess = ReadFile(g_hInputFile, chBuf, BUFSIZ, &dwRead, nullptr);
		if (!bSuccess || dwRead == 0) break;

		bSuccess = WriteFile(g_hChildStd_IN_Wr, chBuf, dwRead, &dwWritten, nullptr);
		if (!bSuccess) break;
	}

	// close the pipe handle so the child process stops reading
	if (!CloseHandle(g_hChildStd_IN_Wr))
		ErrorExit(TEXT("StdInWr CloseHandle"));
}

void ReadFromPipe(void)
{
	// Read output from the child process's pipe for STDOUT
	// and write to the parent process's pipe for STDOUT. 
	// Stop when there is no more data. 
	DWORD dwRead, dwWritten;
	CHAR chBuf[BUFSIZ];
	BOOL bSuccess = FALSE;
	const HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	for (;;)
	{
		bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZ, &dwRead, nullptr);
		if (!bSuccess || dwRead == 0) break;

		bSuccess = WriteFile(hParentStdOut, chBuf,
			dwRead, &dwWritten, nullptr);
		if (!bSuccess) break;
	}
}

void ErrorExit(const char* lpszFunction)
{
	// Format a readable error message, display a message box, 
	// and exit from the application.
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	const DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPTSTR>(&lpMsgBuf),
		0, nullptr);

	lpDisplayBuf = static_cast<LPVOID>(LocalAlloc(LMEM_ZEROINIT,(lstrlen(static_cast<LPCTSTR>(lpMsgBuf)) + lstrlen(static_cast<LPCTSTR>(lpszFunction)) + 40) * sizeof(TCHAR)));

	StringCchPrintf(static_cast<LPTSTR>(lpDisplayBuf),
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(nullptr, static_cast<LPCTSTR>(lpDisplayBuf), TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(1);
}