#include <Windows.h>

#include "GameApplication.h"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	GameApplication application;
	application.Initialize();
	application.Run();
	application.Finalize();
	return 0;
}
