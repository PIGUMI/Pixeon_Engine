#include <windows.h>
#include <iostream>


bool CallAssetPacker(const std::string& toolPath, const std::string& assetDir, const std::string& outputPak) {
    std::string cmd = "\"" + toolPath + "\" \"" + assetDir + "\" \"" + outputPak + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL result = CreateProcessA(
        nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    if (!result) return false;

    // プロセス終了まで待機
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exitCode == 0);
}

int main()
{
	CallAssetPacker("Asset packaging tool.exe", "Assets", "assets.PixAssets");
}