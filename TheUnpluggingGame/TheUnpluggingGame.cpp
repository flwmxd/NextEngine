// TheUnpluggingGame.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "game.h"

#include <ecs/system.h>

extern "C" {
	void register_components_and_systems(World& world) {
		register_default_systems_and_components(world);
	}
}