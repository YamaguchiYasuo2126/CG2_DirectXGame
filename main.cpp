#include <Windows.h>

#include "GameApplication.h"
#include "D3DResourceLeakChecker.h"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// 最初に宣言することで、一番最後にデストラクタが呼ばれる
	D3DResourceLeakChecker leakCheck;

	GameApplication application;
	application.Initialize();
	application.Run();
	application.Finalize();
	return 0;
}
