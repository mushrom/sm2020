#pragma once

#include <grend/gameState.hpp>
#include <grend/gameView.hpp>
#include <grend/gameMain.hpp>
#include <grend/gameModel.hpp>
#include <grend/modalSDLInput.hpp>
#include <grend/physics.hpp>
#include <grend/vecGUI.hpp>

#include <memory>
#include <unordered_map>

namespace grendx {

class testgameView : public gameView {
	public:
		typedef std::shared_ptr<testgameView> ptr;
		typedef std::weak_ptr<testgameView>   weakptr;
		enum modes {
			MainMenu,
			Move,
			Pause,
			GameOver,
			YouWon,
		};

		testgameView(gameMain *game);
		virtual void handleInput(gameMain *game, SDL_Event& ev);
		virtual void render(gameMain *game);
		void logic(gameMain *game, float delta);

		gameObject::ptr cameraObj = std::make_shared<gameObject>();
		gameObject::ptr enemies = std::make_shared<gameObject>();
		gameObject::ptr pickups = std::make_shared<gameObject>();
		gameObject::ptr bullets = std::make_shared<gameObject>();
		gameObject::ptr keyObjs = std::make_shared<gameObject>();

		uint64_t cameraPhysID;
		modalSDLInput input;

		vecGUI vgui;
		int menuSelect = 0;

	private:
		std::pair<gameImport::ptr, model_map> healthPickup
			= load_gltf_scene("assets/healthpickup.gltf");
		std::pair<gameImport::ptr, model_map> turret
			= load_gltf_scene("assets/turret-experiment.gltf");
		std::pair<gameImport::ptr, model_map> keyObj
			= load_gltf_scene("assets/key.gltf");
		gameModel::ptr bulletObj
			= load_object(GR_PREFIX "assets/obj/smoothsphere.obj");

		void updateEnemies(gameMain *game);
		void updateBullets(gameMain *game);
		void updatePickups(gameMain *game);

		void drawMainMenu(gameMain *game, int wx, int wy);
		void drawPauseMenu(int wx, int wy);
		void drawUIStuff(int wx, int wy);
		void drawGameOver(gameMain *game, int wx, int wy);
		void drawWinScreen(gameMain *game, int wx, int wy);
		void generateEnemies(unsigned n = 16);
		void generatePickups(unsigned n = 5);
		void generateKeys(void);
		void invalidateLightMaps(gameMain *game);
		void resetGame(gameMain *game);
		void resetLevel(gameMain *game);

		struct {
			channelBuffers_ptr weapon;
			channelBuffers_ptr pickup;
			channelBuffers_ptr nextLevel;
			channelBuffers_ptr damage;
			channelBuffers_ptr hit;
		} sounds;

		double last_shots[16] = {0.0};
		double health = 1.0;
		double ammo = 1.0;
		unsigned current_level = 0;
		uint16_t keysgot = 1;
		std::unordered_map<uint64_t, float> bulletCreated;
};

// namespace grendx
}
