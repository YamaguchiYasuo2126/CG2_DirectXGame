#pragma once
#include <string>

// UTF-8とUTF-16を変換し、ログやファイルパスをWin32 APIへ渡せる形式にする。
std::wstring ConvertString(const std::string& str);
std::string ConvertString(const std::wstring& str);
