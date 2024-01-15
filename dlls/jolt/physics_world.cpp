// HL includes
#include "extdll.h"
#include "util.h"
#include "game.h"

// Physics includes
#include "physics_util.h"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/PhysicsMaterialSimple.h>

// STL includes
#include <iostream>
#include <cstdarg>
#include <thread>
#include <fstream>

using namespace JPH;
using namespace JPH::literals;

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

	Float3 gravity(0, 0, -g_psv_gravity->value);
	physics_system->SetGravity(Vec3(gravity));
}

void load_physics_world_geometry_OBJ(const std::string &path)
{
	BodyInterface& body_interface = physics_system->GetBodyInterface();

	std::vector<JPH::Vec3> vertices;
	TriangleList triangles;

	// We suck, just load collision from an OBJ instead
	std::ifstream collision_obj_file(path);
	// Skip blender header lines
	char dummy[1024];
	dummy[0] = '\0';
	collision_obj_file.getline(dummy, 1024);
	collision_obj_file.getline(dummy, 1024);

	while (collision_obj_file.good())
	{
		std::string line_header = "";
		collision_obj_file >> line_header;

		if (line_header == "v")
		{
			// Add to vertices
			float x = 0, y = 0, z = 0;
			collision_obj_file >> x >> y >> z;
			JPH::Float3 vertex_floats(x, y, z);
			JPH::Vec3 vertex(vertex_floats);
			vertices.push_back(vertex);
		}

		else if (line_header == "f")
		{
			// Create triangle from vertices
			int v1 = 0, v2 = 0, v3 = 0;
			collision_obj_file >> v1 >> v2 >> v3;
			Triangle triangle(vertices[v1-1], vertices[v2-1], vertices[v3-1]);
			triangles.push_back(triangle);
		}
	}

	// Create material
	PhysicsMaterialList materials;
	materials.push_back(new PhysicsMaterialSimple("Material 0", Color::sGetDistinctColor(0)));

	// Create mesh and body settings
	MeshShapeSettings* body_shape_settings = new MeshShapeSettings(triangles, std::move(materials));
	BodyCreationSettings body_settings(body_shape_settings, RVec3(0.0_r, 0.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
	// body_settings.ConvertShapeSettings();

	// Create body
	body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

	physics_system->OptimizeBroadPhase();
}

void update_physics_world()
{
	// Step the world
	physics_system->Update(physics_delta_time, cCollisionSteps, temp_allocator, job_system);
}

void cleanup_physics_world()
{
	ALERT(at_console, "JOLT: Cleaning up Physics World\n");

	BodyInterface& body_interface = physics_system->GetBodyInterface();

	// Unregisters all types with the factory and cleans up the default material
	UnregisterTypes();

	// Destroy the factory
	delete Factory::sInstance;
	Factory::sInstance = nullptr;

	delete(temp_allocator); temp_allocator = NULL;
	delete(job_system); job_system = NULL;
}
