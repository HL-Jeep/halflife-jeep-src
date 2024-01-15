#pragma once

#include "bspguy/Bsp.h"

void init_physics_world();
void load_physics_world_geometry_OBJ(const std::string& path);
void update_physics_world();
void unload_all();
void cleanup_physics_world();
