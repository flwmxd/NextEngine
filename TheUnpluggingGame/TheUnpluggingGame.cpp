// TheUnpluggingGame.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "game.h"

#include <ecs/system.h>
#include <components/transform.h>
#include <graphics/assets/model.h>
#include <graphics/rhi/window.h>

#include "playerInput.h"
#include "fpsController.h"
#include "bowWeapon.h"

#include "core/reflection.h"
#include "components/transform.h"
#include "engine/application.h"
#include "engine/engine.h"

struct Game {
	PlayerInputSystem player_input;
	FPSControllerSystem fps_controller;
	BowSystem bow_system;
};

APPLICATION_API Game* init(Engine& engine) {
	return new Game{};
}

APPLICATION_API bool is_running(Game& game, Engine& engine) {
	return !engine.window.should_close();
}

APPLICATION_API void update(Game& game, Engine& engine) {
	World& world = engine.world;
	UpdateCtx ctx(engine.time, engine.input);

	game.player_input.update(world, ctx);
	game.fps_controller.update(world, ctx);
	game.bow_system.update(world, ctx);
}

APPLICATION_API void render(Game& game, Engine& engine) {
}

APPLICATION_API void deinit(Game* game, Engine& engine) {
	delete game;
}