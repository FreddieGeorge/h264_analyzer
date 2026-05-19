#include <windows.h>

#include <string>

namespace
{
std::wstring quote(const std::wstring &value)
{
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L"\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring parentDirectory(const std::wstring &path)
{
    const std::wstring::size_type slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L".";
    }
    return path.substr(0, slash);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int)
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        MessageBoxW(nullptr, L"Failed to locate ZStreamEye launcher.", L"ZStreamEye", MB_ICONERROR | MB_OK);
        return 1;
    }

    const std::wstring root = parentDirectory(modulePath);
    const std::wstring appPath = root + L"\\runtime\\ZStreamEyeApp.exe";
    DWORD attributes = GetFileAttributesW(appPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        MessageBoxW(nullptr, L"runtime\\ZStreamEyeApp.exe was not found.", L"ZStreamEye", MB_ICONERROR | MB_OK);
        return 1;
    }

    std::wstring fullCommandLine = quote(appPath);
    if (commandLine != nullptr && commandLine[0] != L'\0') {
        fullCommandLine += L" ";
        fullCommandLine += commandLine;
    }

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};
    std::wstring workingDirectory = root;
    const BOOL started = CreateProcessW(appPath.c_str(),
                                        fullCommandLine.data(),
                                        nullptr,
                                        nullptr,
                                        FALSE,
                                        0,
                                        nullptr,
                                        workingDirectory.data(),
                                        &startupInfo,
                                        &processInfo);
    if (!started) {
        MessageBoxW(nullptr, L"Failed to start ZStreamEye.", L"ZStreamEye", MB_ICONERROR | MB_OK);
        return 1;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 0;
}
