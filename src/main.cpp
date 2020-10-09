#include <testgame/playerView.hpp>

#include <grend/gameMainDevWindow.hpp>
#include <grend/gameMainWindow.hpp>
#include <grend/gameObject.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>

#include <iostream>
#include <memory>

using namespace grendx;

// TODO: library functions to handle most of this
void addCameraWeapon(gameView::ptr view) {
	testgameView::ptr player = std::static_pointer_cast<testgameView>(view);
	gameObject::ptr testweapon = std::make_shared<gameObject>();

	auto [objs, models] =
		load_gltf_scene(GR_PREFIX "assets/obj/Mossberg-lowres/shotgun.gltf");

	std::cerr << GR_PREFIX "assets/obj/Mossberg-lowres/shotgun.gltf" << std::endl;

	compile_models(models);
	bind_cooked_meshes();
	objs->transform.scale = glm::vec3(2.0f);
	objs->transform.position = glm::vec3(-0.3, 1.1, 1.25);
	objs->transform.rotation = glm::quat(glm::vec3(0, 3.f*M_PI/2.f, 0));
	setNode("weapon", player->cameraObj, objs);
}

int main(int argc, char *argv[]) {
	//gameMain *game = new gameMainDevWindow();
	gameMain *game = new gameMainWindow();

	if (argc < 2) {
		game->state->rootnode = loadMap(game);
	} else {
		game->state->rootnode = loadMap(game, argv[1]);
	}

	game->phys->add_static_models(game->state->rootnode);
	gameView::ptr player = std::make_shared<testgameView>(game);
	game->setView(player);
	addCameraWeapon(player);

	/*
	auto foo = openSpatialLoop(GR_PREFIX "assets/sfx/Bit Bit Loop.ogg");
	foo->worldPosition = glm::vec3(-10, 0, -5);
	game->audio->add(foo);

	auto bar = openSpatialLoop(GR_PREFIX "assets/sfx/Meditating Beat.ogg");
	bar->worldPosition = glm::vec3(0, 0, -5);
	game->audio->add(bar);
	*/

	auto buzz = openStereoLoop("./assets/groovething.ogg");
	game->audio->add(buzz);

	game->run();

	std::cout << "Things loaded, we are go" << std::endl;
	return 0;
}
