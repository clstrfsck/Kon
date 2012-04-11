// Kon.cpp : Defines the entry point for the console application.
//
// Tries not to link with any of the MSVC standard libs in order
// to reduce executable size.

#define _WIN32_WINNT 0x0501     // Need XP or better

#include "windows.h"

// Size of buffer for copying input to output for stdin/out/err
#define COPYBUFSIZ  1024

// Size of stack for copy threads
// Standard 1Mb is a little bit over the top for this
#define STACKSIZ    8192

DWORD lengthOf(const char *what) {
    DWORD count;
    for (count = 0; *what; what += 1)
        count += 1;
    return count;
}

void fail(const char *what) {
    static const char * const CRLF = "\r\n";
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    WriteFile(err, what, lengthOf(what), NULL, NULL);
    WriteFile(err, CRLF, lengthOf(CRLF), NULL, NULL);
    ExitProcess(1);
}

DWORD WINAPI threadCopyData(LPVOID lpParameter) {
    HANDLE *handles = (HANDLE *)lpParameter;
    HANDLE input = handles[0];
    HANDLE output = handles[1];
    unsigned char bytes[COPYBUFSIZ];
    DWORD bytesRead;

    // Set input mode to be not line buffered, no echo. Assume this
    // fails without side effects if "input" is not a console handle.
    SetConsoleMode(input, 0);
    while (ReadFile(input, bytes, COPYBUFSIZ, &bytesRead, NULL)) {
        DWORD bytesWritten;
        if (bytesRead > 0 && !WriteFile(output, bytes, bytesRead, &bytesWritten, NULL))
            break;
    }
    return 0;
}

HANDLE createCopyThread(HANDLE handles[2]) {
    HANDLE threadHandle = CreateThread(NULL, STACKSIZ, &threadCopyData, handles, 0, 0);
    if (threadHandle == NULL)
        fail("CreateThread");
    return threadHandle;
}

void createOutputPipe(LPHANDLE parent, LPHANDLE child, LPSECURITY_ATTRIBUTES secAttr) {
    if (!CreatePipe(parent, child, secAttr, 0))
        fail("CreatePipe");
    if (!SetHandleInformation(*parent, HANDLE_FLAG_INHERIT, 0))
        fail("SetHandleInformation");
}

LPTSTR retrieveCommandLine() {
    LPTSTR cmdLine = GetCommandLine();
    if (*cmdLine == '\"') {
        cmdLine += 1;
        while (*cmdLine) {
            if (*cmdLine++ == '\"')
                break;
        }
    } else {
        while (*cmdLine) {
            TCHAR ch = *cmdLine++;
            if (ch == ' ' || ch == '\t')
                break;
        }
    }
    while (*cmdLine == ' ' || *cmdLine == '\t')
        cmdLine += 1;
    if (*cmdLine == '\0')
        fail("No program specified on command line");
    return cmdLine;
}


void WINAPI entryPoint() {
    LPTSTR cmdLine = retrieveCommandLine();
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    HANDLE stdinChild;
    HANDLE stdinHandles[2]  = { GetStdHandle(STD_INPUT_HANDLE), 0  };
    HANDLE stdoutChild;
    HANDLE stdoutHandles[2] = { 0, GetStdHandle(STD_OUTPUT_HANDLE) };
    HANDLE stderrChild;
    HANDLE stderrHandles[2] = { 0, GetStdHandle(STD_ERROR_HANDLE)  };

    {
        // Create anon pipes for stdin/out/err
        SECURITY_ATTRIBUTES secAttr;
        secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        secAttr.bInheritHandle = TRUE;
        secAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&stdinChild, &stdinHandles[1], &secAttr, 0))
            fail("CreatePipe");
        if (!SetHandleInformation(stdinHandles[1], HANDLE_FLAG_INHERIT, 0))
            fail("SetHandleInformation");

        createOutputPipe(&stdoutHandles[0], &stdoutChild, &secAttr);
        createOutputPipe(&stderrHandles[0], &stderrChild, &secAttr);
    }

    GetStartupInfo(&startupInfo);
    startupInfo.dwFlags   |= STARTF_USESTDHANDLES;
    startupInfo.hStdInput  = stdinChild;
    startupInfo.hStdOutput = stdoutChild;
    startupInfo.hStdError  = stderrChild;

    if (CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        DWORD exitCode;
        createCopyThread(stdinHandles);
        createCopyThread(stdoutHandles);
        createCopyThread(stderrHandles);
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        if (GetExitCodeProcess(processInfo.hProcess, &exitCode))
            ExitProcess(exitCode);
        else
            fail("GetExitCodeProcess");
    }
    fail("CreateProcess");
}

// EOF
