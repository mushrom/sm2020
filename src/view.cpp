#include <testgame/playerView.hpp>
#include <grend/gameView.hpp>
#include <grend/audioMixer.hpp>
#include <grend/controllers.hpp>
#include <glm/gtx/rotate_vector.hpp>

using namespace grendx;

static renderPostStage<rOutput>::ptr testpost = nullptr;
static gameObject::ptr testweapon = nullptr;

#include <random>
static std::default_random_engine generator;
static std::uniform_real_distribution<float> distribution(0.0, 1.0);

static float randomComponent(void) {
	float x = 10*distribution(generator);
	return (x > 0)? 30.0 + x : -30.0 - x;
}

static glm::vec3 randomPosition(void) {
	float a = 2*3.1415926*distribution(generator);
	float x = sin(a);
	float y = cos(a);
	float adjx = 9.f*distribution(generator);
	float adjy = 9.f*distribution(generator);

	return glm::vec3((45.0 - adjx) * x, -3, (45.0 - adjy) * y);
}

#define MAX_LEVELS 4
static inline unsigned nextLevel(unsigned n) {
	return (n + 1) % MAX_LEVELS;
}

struct nvg_data {
	int fontNormal, fontBold, fontIcons, fontEmoji;
};

testgameView::testgameView(gameMain *game) : gameView() {
	static const float speed = 15.f;

	compile_models(healthPickup.second);
	compile_models(turret.second);
	compile_models(keyObj.second);
	compile_model("bulletObj", bulletObj);
	bind_cooked_meshes();

	testpost = makePostprocessor<rOutput>(game->rend->shaders["post"],
		SCREEN_SIZE_X, SCREEN_SIZE_Y);

	cameraPhysID = game->phys->add_sphere(cameraObj, glm::vec3(0, 2, 0), 1.0, 1.0);
	setNode("player camera", game->state->physObjects, cameraObj);

	setNode("bullets", game->state->rootnode, bullets);
	setNode("enemies", game->state->rootnode, enemies);
	setNode("pickups", game->state->rootnode, pickups);
	setNode("keyObjs", game->state->rootnode, keyObjs);

	sounds.weapon    = openAudio("assets/phaser.ogg");
	sounds.pickup    = openAudio("assets/healthpickup.ogg");
	sounds.nextLevel = openAudio("assets/next.ogg");
	sounds.damage    = openAudio("assets/oof.ogg");
	sounds.hit       = openAudio("assets/turrethit.ogg");

	input.bind(modes::MainMenu,
		[&] (SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				switch (ev.key.keysym.sym) {
					case SDLK_DOWN:
					case SDLK_j:
						menuSelect++;
						break;

					case SDLK_UP:
					case SDLK_k:
						menuSelect = max(0, menuSelect - 1);
						break;

					case SDLK_RETURN:
					case SDLK_SPACE:
						if (menuSelect == 0) {
							return (int)modes::Move;
						}
						break;

					default:
						break;
				}
			}

			return MODAL_NO_CHANGE;
		});

	input.bind(modes::Pause,
		[&] (SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				switch (ev.key.keysym.sym) {
					case SDLK_DOWN:
					case SDLK_j:
						menuSelect++;
						break;

					case SDLK_UP:
					case SDLK_k:
						menuSelect = max(0, menuSelect - 1);
						break;

					case SDLK_RETURN:
					case SDLK_SPACE:
						if (menuSelect == 0) {
							return (int)modes::Move;
						} else if (menuSelect == 1) {
							return (int)modes::MainMenu;
						}
						break;

					default:
						break;
				}
			}

			return MODAL_NO_CHANGE;
		});
	input.bind(modes::Move, controller::camMovement(cam, 10.f));
	input.bind(modes::Move, controller::camFPS(cam, game));

	// switch modes (TODO: add pause)
	input.bind(modes::Move,
		[&] (SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_KEYDOWN) {
				switch (ev.key.keysym.sym) {
					// TODO: should pause
					case SDLK_ESCAPE: return (int)modes::Pause;
					default: break;
				};
			}
			return MODAL_NO_CHANGE;
		});

	// TODO: this could be a function generator
	input.bind(modes::Move,
		[&, game] (SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_MOUSEBUTTONDOWN
			    && ev.button.button == SDL_BUTTON_LEFT)
			{
				if (ammo > 0.15) {
					float ticks = SDL_GetTicks() / 1000.f;

					auto ptr = std::make_shared<spatialAudioChannel>(sounds.weapon);
					ptr->worldPosition = cam->position() + cam->direction();
					game->audio->add(ptr);

					gameObject::ptr obj = std::make_shared<gameObject>();
					glm::vec3 off = glm::normalize(cam->direction())*3.f;
					obj->transform.position = cam->position();
					obj->transform.scale = glm::vec3(0.5f);
					obj->transform.position += off;

					obj->id = game->phys->add_sphere(obj, obj->transform.position, 1.0, 0.5);

					glm::vec3 camvel = game->phys->get_velocity(cameraPhysID);
					game->phys->set_velocity(obj->id, glm::normalize(cam->direction())*40.f + camvel);

					bulletCreated[obj->id] = ticks;

					std::string name = "bullet" + std::to_string(obj->id);
					setNode("model", obj, bulletObj);
					setNode(name, bullets, obj);

					ammo = max(0.f, ammo - 0.15);
				}
			}

			return MODAL_NO_CHANGE;
		});

	// TODO: this could be a function generator
	input.bind(modes::Move,
		[&, game] (SDL_Event& ev, unsigned flags) {
			if (ev.type == SDL_MOUSEBUTTONDOWN
			    && ev.button.button == SDL_BUTTON_RIGHT)
			{
				unsigned next = nextLevel(current_level);
				if (keysgot == (1 << MAX_LEVELS) - 1) {
					input.setMode(modes::YouWon);
				}

				else if (glm::length(cam->position()) < 10.f && (keysgot & (1 << next))) {
					current_level = next;
					resetLevel(game);

					auto ptr = std::make_shared<stereoAudioChannel>(sounds.nextLevel);
					game->audio->add(ptr);
				}
			}

			return MODAL_NO_CHANGE;
		});

	input.setMode(modes::MainMenu);
}

void testgameView::handleInput(gameMain *game, SDL_Event& ev) {
	input.dispatch(ev);
}

void testgameView::logic(gameMain *game, float delta) {
	if (input.mode == modes::Move && health < 0.02) {
		input.setMode(modes::GameOver);
	}

	// TODO: cam->update(delta);
	else if (input.mode == modes::Move) {
		ammo = min(1.f, ammo + delta*0.15); // 15% ammo every second 
		health = min(1.f, health + delta*0.02); // recover 2% health every second

		game->phys->set_acceleration(cameraPhysID, cam->velocity());
		game->phys->step_simulation(1.f/game->frame_timer.last());

		cam->setPosition(cameraObj->transform.position + glm::vec3(0, 1.5, 0));
		cameraObj->transform.rotation = glm::quat(glm::vec3(
					// pitch, yaw, roll
					cam->direction().y*-0.5f,
					atan2(cam->direction().x, cam->direction().z),
					0));

		updateEnemies(game);
		updateBullets(game);
		updatePickups(game);
	}
}

void testgameView::updateEnemies(gameMain *game) {
	float ticks = SDL_GetTicks() / 1000.f;

	for (auto& [name, ptr] : enemies->nodes) {
		glm::vec3 dir = ptr->transform.position - cam->position();
		glm::quat rot = glm::quat(glm::vec3(0, atan2(dir.x, dir.z), 0));
		float height = min(0.5f, max(0.f, 25.f - glm::length(dir))*0.2f);

		auto head = ptr->getNode("head");
		assert(head != nullptr);
		head->transform.rotation = rot;
		head->transform.position = glm::vec3(0, height, 0);

		if (glm::length(dir) < 20.0 && (ticks - last_shots[ptr->id]) > 1.5f) {
			std::cout << "turret " << ptr->id << " shot a bullet!" << std::endl;
			last_shots[ptr->id] = ticks;

			gameObject::ptr obj = std::make_shared<gameObject>();
			obj->transform.position =
				ptr->transform.position + glm::vec3(0, 2.f, 0);
			obj->transform.scale = glm::vec3(0.5f);

			glm::vec3 adjdir = obj->transform.position - cam->position();
			obj->transform.position += -glm::normalize(adjdir)*3.f; 

			obj->id = game->phys->add_sphere(obj, obj->transform.position, 1.0, 0.5);
			game->phys->set_velocity(obj->id, -glm::normalize(adjdir)*40.f);

			bulletCreated[obj->id] = ticks;

			std::string name = "bullet" + std::to_string(obj->id);
			setNode("model", obj, bulletObj);
			setNode(name, bullets, obj);

			auto ptr = std::make_shared<spatialAudioChannel>(sounds.weapon);
			ptr->worldPosition = obj->transform.position;
			game->audio->add(ptr);
		}
	}
}

void testgameView::updateBullets(gameMain *game) {
	float ticks = SDL_GetTicks() / 1000.f;

	for (auto it = bullets->nodes.begin(); it != bullets->nodes.end();) {
		auto& [name, ptr] = *it;
		auto created = bulletCreated.find(ptr->id);

		if (created != bulletCreated.end() && ticks - created->second >= 3.0) {
			it = bullets->nodes.erase(it);
			bulletCreated.erase(created);

		} else if (glm::distance(ptr->transform.position, cam->position()) < 1.f) {
			it = bullets->nodes.erase(it);
			// TODO: need to implement imp_physics::remove()
			game->phys->remove(ptr->id);
			std::cout << "hit by bullet " << name << std::endl;
			health = max(0.f, health - 0.22);
			std::cout << "current health: " << health << std::endl;

			auto ptr = std::make_shared<stereoAudioChannel>(sounds.damage);
			game->audio->add(ptr);

		} else {
			bool enemyHit = false;
			for (auto en = enemies->nodes.begin();
					en != enemies->nodes.end();)
			{
				auto& [ename, eptr] = *en;

				if (glm::distance(eptr->transform.position + glm::vec3(0, 2, 0), ptr->transform.position) < 1.f) {
					it = bullets->nodes.erase(it);
					en = enemies->nodes.erase(en);
					game->phys->remove(ptr->id);
					enemyHit = true;
					invalidateLightMaps(game);

					auto ptr = std::make_shared<spatialAudioChannel>(sounds.hit);
					ptr->worldPosition = eptr->transform.position;
					game->audio->add(ptr);

					break;

				} else {
					en++;
				}
			}

			if (!enemyHit) it++;
		}
	}
}

void testgameView::updatePickups(gameMain *game) {
	for (auto it = pickups->nodes.begin(); it != pickups->nodes.end();) {
		auto& [name, ptr] = *it;

		if (glm::distance(ptr->transform.position, cam->position()) < 2.f) {
			it = pickups->nodes.erase(it);
			// TODO: need to implement imp_physics::remove()
			game->phys->remove(ptr->id);
			std::cout << "picked up " << name << std::endl;
			health = min(1.f, health + 0.25);
			std::cout << "current health: " << health << std::endl;

			auto ptr = std::make_shared<stereoAudioChannel>(sounds.pickup);
			game->audio->add(ptr);

		} else {
			it++;
		}
	}

	for (auto it = keyObjs->nodes.begin(); it != keyObjs->nodes.end();) {
		auto& [name, ptr] = *it;

		if (glm::distance(ptr->transform.position, cam->position()) < 3.f) {
			it = keyObjs->nodes.erase(it);
			// TODO: need to implement imp_physics::remove()
			game->phys->remove(ptr->id);
			std::cout << "got key! " << ptr->id + 1 << std::endl;
			keysgot |= (1 << ptr->id);

			auto ptr = std::make_shared<stereoAudioChannel>(sounds.pickup);
			game->audio->add(ptr);

		} else {
			it++;
		}
	}
}

void testgameView::drawUIStuff(int wx, int wy) {
	NVGcontext *vg = vgui.nvg;

	Framebuffer().bind();
	set_default_gl_flags();

	disable(GL_DEPTH_TEST);
	disable(GL_SCISSOR_TEST);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	nvgBeginFrame(vg, wx, wy, 1.0);
	nvgSave(vg);

	float ticks = SDL_GetTicks() / 1000.f;

	if (glm::distance(cam->position(), glm::vec3(0)) < 10.0) {
		nvgFontSize(vg, 32.f);
		nvgFontFace(vg, "sans-bold");
		nvgFontBlur(vg, 0);
		nvgTextAlign(vg, NVG_ALIGN_CENTER);

		if (keysgot & (1 << nextLevel(current_level))) {
			nvgFillColor(vg, nvgRGBA(0x60, 0xf0, 0x60, 224));
			nvgText(vg, wx/2, wy - 80 - 16*sin(4*ticks),
					"[right click] Next Level", NULL);
		} else {
			nvgFillColor(vg, nvgRGBA(0xf0, 0x60, 0x60, 224));
			nvgText(vg, wx/2, wy - 80 - 16*sin(4*ticks),
					"❎ Need key!", NULL);
		}
		//nvgFillColor(vg, nvgRGBA(220, 220, 220, 160));
	}

	nvgBeginPath(vg);
	nvgRect(vg, wx/2 - 4, wy/2, 9, 1);
	nvgRect(vg, wx/2, wy/2 - 4, 1, 9);
	nvgFillColor(vg, nvgRGBA(16, 172, 16, 192));
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, 48, 35, 16, 42, 3);
	nvgRoundedRect(vg, 35, 48, 42, 16, 3);
	nvgFillColor(vg, nvgRGBA(172, 16, 16, 192));
	nvgFill(vg);

	//nvgRotate(vg, 0.1*cos(ticks));
	nvgBeginPath(vg);
	nvgRect(vg, 90, 44, 256, 24);
	nvgFillColor(vg, nvgRGBA(30, 30, 30, 127));
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgRect(vg, 93, 47, 250*health, 16);
	nvgFillColor(vg, nvgRGBA(192, 32, 32, 127));
	nvgFill(vg);

	//nvgRotate(vg, 0.1*cos(ticks));
	nvgBeginPath(vg);
	nvgRect(vg, 90, 44 + 24, 256, 24);
	nvgFillColor(vg, nvgRGBA(30, 30, 30, 127));
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgRect(vg, 93, 47 + 24, 250*ammo, 16);
	nvgFillColor(vg, nvgRGBA(32, 32, 192, 127));
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, wx - 250, 50, 200, 16 * (MAX_LEVELS + 4), 10);
	nvgFillColor(vg, nvgRGBA(28, 30, 34, 192));
	nvgFill(vg);

	nvgFontSize(vg, 16.f);
	nvgFontFace(vg, "sans-bold");
	nvgFontBlur(vg, 0);
	nvgTextAlign(vg, NVG_ALIGN_LEFT);
	nvgFillColor(vg, nvgRGBA(0xf0, 0x60, 0x60, 160));
	nvgText(vg, wx - 82, 80, "❎", NULL);
	nvgFillColor(vg, nvgRGBA(220, 220, 220, 160));

	/*
	nvgText(vg, wx - 235, 80, "💚 Testing this", NULL);
	nvgText(vg, wx - 235, 80 + 16, "Go forward ➡", NULL);
	nvgText(vg, wx - 235, 80 + 32, "⬅ Go back", NULL);
	*/

	{
		std::string txt = "Current level: " + std::to_string(current_level + 1);
		nvgFillColor(vg, nvgRGBA(220, 220, 220, 160));
		nvgText(vg, wx - 235, 80, txt.c_str(), NULL);
	}

	for (unsigned i = 0; i < MAX_LEVELS; i++) {
		if (keysgot & (1 << i)) {
			nvgFillColor(vg, nvgRGBA(0xa0, 0xe0, 0xa0, 192));
		} else {
			nvgFillColor(vg, nvgRGBA(0xe0, 0xa0, 0xa0, 192));
		}

		std::string txt = "💚 + key " + std::to_string(i+1);
		nvgText(vg, wx - 235, 80 + (2+i)*16, txt.c_str(), NULL);
	}

	nvgRestore(vg);
	nvgEndFrame(vg);
}

void testgameView::drawMainMenu(gameMain *game, int wx, int wy) {
	int center = wx/2;
	vgui.newFrame(wx, wy);
	vgui.menuBegin(center - 150, 100, 300, "Space Marines 2020");

	if (vgui.menuEntry("🔫 Start", &menuSelect)) {
		if (vgui.clicked()) {
			resetGame(game);
			input.mode = modes::Move;

		} else if (vgui.hovered()) {
			menuSelect = vgui.menuCount();
		}
	}

	vgui.menuEnd();
	vgui.endFrame();
}

void testgameView::drawGameOver(gameMain *game, int wx, int wy) {
	int center = wx/2;
	vgui.newFrame(wx, wy);
	vgui.menuBegin(center - 150, 100, 300, "LOL you died");
	if (vgui.menuEntry("Retry", &menuSelect)) {
		if (vgui.clicked()) {
			//resetGame(game);
			input.mode = modes::MainMenu;

		} else if (vgui.hovered()) {
			menuSelect = vgui.menuCount();
		}
	}

	vgui.menuEnd();
	vgui.endFrame();
}

void testgameView::drawWinScreen(gameMain *game, int wx, int wy) {
	int center = wx/2;
	vgui.newFrame(wx, wy);
	vgui.menuBegin(center - 150, 100, 300, "You are winnar !");
	if (vgui.menuEntry("Play again", &menuSelect)) {
		if (vgui.clicked()) {
			//resetGame(game);
			input.mode = modes::MainMenu;

		} else if (vgui.hovered()) {
			menuSelect = vgui.menuCount();
		}
	}

	vgui.menuEnd();
	vgui.endFrame();
}

void testgameView::drawPauseMenu(int wx, int wy) {
	int center = wx/2;
	vgui.newFrame(wx, wy);
	vgui.menuBegin(center - 150, 100, 300, "Paused");
	if (vgui.menuEntry("Resume", &menuSelect)) {
		if (vgui.clicked()) {
			input.mode = modes::Move;

		} else if (vgui.hovered()) {
			menuSelect = vgui.menuCount();
		}
	}

	if (vgui.menuEntry("Quit", &menuSelect)) {
		if (vgui.clicked()) {
			input.mode = modes::MainMenu;
			// TODO:

		} else if (vgui.hovered()) {
			menuSelect = vgui.menuCount();
		}
	}

	vgui.menuEnd();
	vgui.endFrame();
}

void testgameView::generateEnemies(unsigned n) {
	for (unsigned i = 0; i < n; i++) {
		std::string name = "turret" + std::to_string(i);
		gameObject::ptr obj = std::make_shared<gameObject>();
		obj->transform.position = randomPosition();
		obj->id = i;

		gameObject::ptr head = std::make_shared<gameObject>();
		gameObject::ptr base = std::make_shared<gameObject>();

		setNode("model", base, turret.first->getNode("scene-root[2]"));
		setNode("model", head, turret.first->getNode("scene-root[3]"));

		setNode("base", obj, base);
		setNode("head", obj, head);
		setNode(name, enemies, obj);
	}
}

void testgameView::generatePickups(unsigned n) {
	for (unsigned i= 0; i < n; i++) {
		std::string name = "pickup" + std::to_string(i);
		gameObject::ptr obj = std::make_shared<gameObject>();
		obj->transform.position = randomPosition() + glm::vec3(0, 1.5, 0);

		setNode("model", obj, healthPickup.first);
		setNode(name, pickups, obj);
	}
}

void testgameView::generateKeys(void) {
	unsigned next = nextLevel(current_level);
	std::string name = "key" + std::to_string(next);
	gameObject::ptr obj = std::make_shared<gameObject>();
	obj->transform.position = randomPosition() + glm::vec3(0, 1.5, 0);
	obj->id = next;

	setNode("model", obj, keyObj.first);
	setNode(name, keyObjs, obj);
}

// XXX: workaround until smart updates are done in the renderer
void testgameView::invalidateLightMaps(gameMain *game) {
	for (auto& [name, ptr] : game->state->rootnode->nodes) {
		if (ptr->type == gameObject::objType::Light) {
			gameLight::ptr lit =
				std::dynamic_pointer_cast<gameLight>(ptr);
			// XXX: invalidate all shadowmaps
			lit->have_map = false;
		}
	}

	// reflection probes
}

void testgameView::resetGame(gameMain *game) {
	std::cerr << "Resetti spaghetti" << std::endl;
	current_level = 0;
	health = ammo = 1.f;
	keysgot = 1;
	game->phys->set_position(cameraPhysID, glm::vec3(0, 2, 0));
	resetLevel(game);
}

void testgameView::resetLevel(gameMain *game) {
	for (auto [id, time] : bulletCreated) {
		game->phys->remove(id);
	}

	invalidateLightMaps(game);
	enemies->nodes.clear();
	bullets->nodes.clear();
	pickups->nodes.clear();

	generateEnemies();
	generatePickups();
	generateKeys();
}


void testgameView::render(gameMain *game) {
	int winsize_x, winsize_y;
	float fticks = SDL_GetTicks() / 1000.f;
	SDL_GetWindowSize(game->ctx.window, &winsize_x, &winsize_y);

	renderWorld(game, cam);
	/*
	renderQueue que(cam);
	que.add(enemies, fticks);
	que.add(pickups, fticks);
	que.add(bullets, fticks);

		game->rend->framebuffer->framebuffer->bind();
		DO_ERROR_CHECK();

		// TODO: constants for texture bindings, no magic numbers floating around
		game->rend->shaders["main"]->bind();
		glActiveTexture(GL_TEXTURE6);
		game->rend->atlases.reflections->color_tex->bind();
		game->rend->shaders["main"]->set("reflection_atlas", 6);
		glActiveTexture(GL_TEXTURE7);
		game->rend->atlases.shadows->depth_tex->bind();
		game->rend->shaders["main"]->set("shadowmap_atlas", 7);
		DO_ERROR_CHECK();

		game->rend->shaders["main"]->set("time_ms", SDL_GetTicks() * 1.f);
		DO_ERROR_CHECK();


	// TODO: this is definitely something there should be a more abstract
	//       wrapper for
	auto flags = game->rend->getFlags();
	que.flush(game->rend->framebuffer, flags, game->rend->atlases);
	*/

	testpost->setSize(winsize_x, winsize_y);
	testpost->draw(game->rend->framebuffer);

	if (input.mode == modes::MainMenu) {
		// TODO: function to do this
		drawMainMenu(game, winsize_x, winsize_y);

	} else if (input.mode == modes::Pause) {
		// TODO: function to do this
		drawPauseMenu(winsize_x, winsize_y);

	} else if (input.mode == modes::GameOver) {
		drawGameOver(game, winsize_x, winsize_y);

	} else if (input.mode == modes::YouWon) {
		drawWinScreen(game, winsize_x, winsize_y);

	} else {
		// TODO: function to do this
		drawUIStuff(winsize_x, winsize_y);
	}
}
