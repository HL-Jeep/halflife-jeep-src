// HL includes
#include "extdll.h"
#include "util.h"

// Physics includes
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
using namespace JPH::literals;

// Test stuff
BodyID sphere_id;
Body* floor_body;

// Callback for traces
static void TraceImpl(const char* inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the HL console
	ALERT(at_console, "JOLT TRACE: %s\n", buffer);
}

// Callback for asserts
#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
	// Print to the HL console
	ALERT(at_console, "JOLT ASSERT FAILED: %s: %s: (%s) %s\n", inFile, inLine, inExpression, (inMessage != nullptr ? inMessage : ""));

	// Breakpoint
	return true;
}
#endif

void init_physics_world()
{
	ALERT(at_console, "JOLT: Initializing Physics World\n");

	// Register allocation hook
	RegisterDefaultAllocator();

	// Install callbacks
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

	// Create a factory
	Factory::sInstance = new Factory();

	// Register all Jolt physics types
	RegisterTypes();

	// Pre-allocate 10MB of memory for use with the physics system
	temp_allocator = new TempAllocatorImpl(10 * 1024 * 1024);

	// Job system that will execute physics jobs on multiple threads.
	job_system = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

	// Init singletons
	broad_phase_layer_interface = new BPLayerInterfaceImpl;
	object_vs_broadphase_layer_filter = new ObjectVsBroadPhaseLayerFilterImpl;
	object_vs_object_layer_filter = new ObjectLayerPairFilterImpl;
	physics_system = new PhysicsSystem;

	// Create the actual physics system.
	physics_system->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, *broad_phase_layer_interface, *object_vs_broadphase_layer_filter, *object_vs_object_layer_filter);

	physics_system->SetBodyActivationListener(&body_activation_listener);
	physics_system->SetContactListener(&contact_listener);

	// The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
	// variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
	BodyInterface &body_interface = physics_system->GetBodyInterface();

	// Next we can create a rigid body to serve as the floor, we make a large box
	// Create the settings for the collision volume (the shape).
	// Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
	BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));

	// Create the shape
	ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
	ShapeRefC floor_shape = floor_shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

	// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
	BodyCreationSettings floor_settings(floor_shape, RVec3(0.0_r, -1.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);

	// Create the actual rigid body
	floor_body = body_interface.CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr

	// Add it to the world
	body_interface.AddBody(floor_body->GetID(), EActivation::DontActivate);

	// Now create a dynamic body to bounce on the floor
	// Note that this uses the shorthand version of creating and adding a body to the world
	BodyCreationSettings sphere_settings(new SphereShape(0.5f), RVec3(0.0_r, 2.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
	sphere_id = body_interface.CreateAndAddBody(sphere_settings, EActivation::Activate);

	// Now you can interact with the dynamic body, in this case we're going to give it a velocity.
	// (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
	body_interface.SetLinearVelocity(sphere_id, Vec3(0.0f, -5.0f, 0.0f));

	physics_system->OptimizeBroadPhase();
}

uint step = 0;
void update_physics_world()
{
	// Next step
	++step;

	BodyInterface& body_interface = physics_system->GetBodyInterface();

	// Output current position and velocity of the sphere
	RVec3 position = body_interface.GetCenterOfMassPosition(sphere_id);
	Vec3 velocity = body_interface.GetLinearVelocity(sphere_id);
	ALERT(at_console, "JOLT: Step %d: Position = (%f, %f, %f), Velocity = (%f, %f, %f)\n", step, position.GetX(), position.GetY(), position.GetZ(), velocity.GetX(), velocity.GetY(), velocity.GetZ());

	// Step the world
	physics_system->Update(physics_delta_time, cCollisionSteps, temp_allocator, job_system);
}

void cleanup_physics_world()
{
	ALERT(at_console, "JOLT: Cleaning up Physics World\n");

	BodyInterface& body_interface = physics_system->GetBodyInterface();

	// Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
	body_interface.RemoveBody(sphere_id);

	// Destroy the sphere. After this the sphere ID is no longer valid.
	body_interface.DestroyBody(sphere_id);

	// Remove and destroy the floor
	body_interface.RemoveBody(floor_body->GetID());
	body_interface.DestroyBody(floor_body->GetID());

	// Unregisters all types with the factory and cleans up the default material
	UnregisterTypes();

	// Destroy the factory
	delete Factory::sInstance;
	Factory::sInstance = nullptr;

	delete(temp_allocator); temp_allocator = NULL;
	delete(job_system); job_system = NULL;
}
