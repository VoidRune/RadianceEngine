#include "App/GameApp.h"

int main(int argc, char** argv)
{
	Game::GameApp app(Game::ParseLaunchOptions(argc, argv));
	return app.Run();
}
