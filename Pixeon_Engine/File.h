#ifndef FILE_H
#define FILE_H

#include <string>

// I/O関連のユーティリティクラス
class File
{
public:
	static void OpenExplorer(const std::string& path);
	static std::string GetExePath();
	static std::string RemoveExeFromPath(const std::string& exePath);
	static bool CallAssetPacker(const std::string& toolPath, const std::string& assetDir, const std::string& outputPak);
	static bool RunArchiveTool(const std::string& toolExePath, const std::string& assetDir, const std::string& archivePath);
	static bool RenameFileInDirectory(const std::string& oldName, const std::string& newName);
};

#endif // !FILE_H

