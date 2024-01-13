#include "physics_util.h"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

// STL includes
#include <iostream>
#include <cstdarg>
#include <thread>

using namespace JPH;

// Singletons

// Create mapping table from object layer to broadphase layer
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
BPLayerInterfaceImpl *broad_phase_layer_interface;

// Create class that filters object vs broadphase layers
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
ObjectVsBroadPhaseLayerFilterImpl *object_vs_broadphase_layer_filter;

// Create class that filters object vs object layers
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
ObjectLayerPairFilterImpl *object_vs_object_layer_filter;

// A body activation listener gets notified when bodies activate and go to sleep
HLBodyActivationListener body_activation_listener;

// A contact listener gets notified when bodies (are about to) collide, and when they separate again.
HLContactListener contact_listener;

// The physics system
PhysicsSystem *physics_system;

TempAllocatorImpl* temp_allocator;
JobSystemThreadPool* job_system;


// Constants

// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
// Note: Max ents in HL1 is 2048, so this value works fine.
const uint cMaxBodies = 2048;

// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
const uint cNumBodyMutexes = 0;

// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
// Note: Max ents in HL1 is 2048, so this value works fine.
const uint cMaxBodyPairs = 2048;

// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
// Note: Max ents in HL1 is 2048, so this value works fine.
const uint cMaxContactConstraints = 2048;

// We simulate the physics world in discrete time steps. For now, assume 100Hz (default fps_max in HL). TODO: Get real update rate (or figure out fixed rate).
const float physics_delta_time = 1.0f / 100.0f;

// If you take larger steps than 1 / 100th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 100th of a second (round up).
const int cCollisionSteps = 1;
