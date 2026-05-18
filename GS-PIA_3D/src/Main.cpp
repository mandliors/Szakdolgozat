#include "App3D/App3D.hpp"

int main()
{
	constexpr float scale = 1.0f;
	constexpr auto screenWidth = static_cast<int>(2000 * scale);
	constexpr auto screenHeight = static_cast<int>(1400 * scale);

	App3D app{screenWidth, screenHeight, "GS-PIA 3D"};

	if (!app.Init())
		return -1;
	app.SetTargetFPS(120);
	app.Run();
	app.Destroy();

	return 0;
}
