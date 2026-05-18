#include "App2D/App2D.hpp"

int main()
{
	constexpr float scale = 1.0f;
	constexpr auto screenWidth = static_cast<int>(2000 * scale);
	constexpr auto screenHeight = static_cast<int>(1400 * scale);

	App2D app{screenWidth, screenHeight, "GS-PIA 2D"};

	if (!app.Init())
		return -1;
	app.SetTargetFPS(120);
	app.Run();
	app.Destroy();

	return 0;
}
