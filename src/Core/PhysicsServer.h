/**
 * @file PhysicsServer.h
 * @brief Server-side Jolt Physics integration for bugmin.
 */

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

class BugminBPLayerInterface;
class BugminObjVsBPLayerFilter;
class BugminObjLayerPairFilter;
class BugminContactListener;
class BugminBodyActivationListener;

/**
 * @namespace ObjectLayers
 * @brief Defines the object layers for physics collisions.
 */
namespace ObjectLayers
{
    inline constexpr JPH::ObjectLayer STATIC   = 0; ///< Static environment geometry.
    inline constexpr JPH::ObjectLayer DYNAMIC  = 1; ///< Dynamic moving objects.
    inline constexpr JPH::ObjectLayer TRIGGER  = 2; ///< Ghost objects for overlap detection.
    inline constexpr uint32_t         COUNT    = 3; ///< Total number of layers.
}

/**
 * @struct PhysicsBodyHandle
 * @brief Thin wrapper so game code never touches raw BodyIDs.
 */
struct PhysicsBodyHandle
{
    JPH::BodyID id; ///< The underlying Jolt BodyID.
    
    /** @brief Checks if the handle points to a valid body. */
    bool IsValid() const { return !id.IsInvalid(); }
    
    /** @brief Equality operator for handle comparison. */
    bool operator==(const PhysicsBodyHandle& o) const { return id == o.id; }
};

/**
 * @struct TransformSnapshot
 * @brief Data structure representing the state of a physics body for network synchronization.
 */
struct TransformSnapshot
{
    uint32_t   entityID   = 0; ///< ID of the entity associated with this body.
    JPH::RVec3 position   = JPH::RVec3::sZero(); ///< World-space position.
    JPH::Quat  rotation   = JPH::Quat::sIdentity(); ///< World-space rotation.
    JPH::Vec3  linearVel  = JPH::Vec3::sZero(); ///< Linear velocity vector.
    JPH::Vec3  angularVel = JPH::Vec3::sZero(); ///< Angular velocity vector.
};

/**
 * @struct PhysicsServerConfig
 * @brief Configuration settings for the PhysicsServer.
 */
struct PhysicsServerConfig
{
    float    fixedTimestep         = 1.0f / 60.0f; ///< The fixed delta time for each physics step.
    int      collisionSteps        = 1; ///< Number of collision sub-steps per update.
    uint32_t maxBodies             = 4096; ///< Maximum number of physics bodies supported.
    uint32_t maxBodyPairs          = 65536; ///< Maximum number of body pairs for broadphase.
    uint32_t maxContactConstraints = 16384; ///< Maximum number of contact constraints.
    uint32_t tempAllocatorBytes    = 20u * 1024u * 1024u; ///< Size of the temporary allocator slab (20 MB).
    int      workerThreads         = -1; ///< Number of worker threads (-1 for auto-detect).
    JPH::Vec3 gravity              = JPH::Vec3(0.f, -9.81f, 0.f); ///< Global gravity vector.
};

/**
 * @class PhysicsServer
 * @brief Manages the Jolt physics system, body lifecycle, and simulation stepping.
 */
class PhysicsServer
{
public:
    using Config = PhysicsServerConfig;

    /**
     * @brief Constructs the PhysicsServer with an optional configuration.
     * @param cfg Configuration settings.
     */
    explicit PhysicsServer(Config cfg = Config{});
    
    /** @brief Cleans up physics resources on destruction. */
    ~PhysicsServer();

    PhysicsServer(const PhysicsServer&)            = delete;
    PhysicsServer& operator=(const PhysicsServer&) = delete;

    /** @brief Initializes the Jolt factory, types, and physics system. */
    void Init();
    
    /** @brief Shuts down the physics system and clears all bodies. */
    void Shutdown();

    /**
     * @brief Advances the physics simulation by a fixed timestep.
     * @param deltaTime The time elapsed since the last frame.
     * @return const std::vector<TransformSnapshot>& List of snapshots for tracked bodies.
     */
    const std::vector<TransformSnapshot>& Step(float deltaTime);

    /**
     * @brief Adds a static floor plane to the world.
     * @param centre Center position of the floor.
     * @param halfExtentX Half-width along the X axis.
     * @param halfExtentZ Half-width along the Z axis.
     * @return PhysicsBodyHandle Handle to the created floor body.
     */
    PhysicsBodyHandle AddStaticFloor(
        JPH::RVec3 centre     = JPH::RVec3(0, 0, 0),
        float halfExtentX = 100.f, float halfExtentZ = 100.f);

    /**
     * @brief Adds a static box to the world.
     * @param position World position.
     * @param halfExtents Dimensions of the box.
     * @param rotation World rotation.
     * @return PhysicsBodyHandle Handle to the created body.
     */
    PhysicsBodyHandle AddStaticBox(
        JPH::RVec3 position,
        JPH::Vec3  halfExtents,
        JPH::Quat  rotation = JPH::Quat::sIdentity());

    /**
     * @brief Adds a static body using an arbitrary pre-built Jolt Shape.
     *
     * This is the primary entry point for mesh collision: build a MeshShape via
     * MeshCollisionBuilder::Build(), then pass the result here.
     *
     * @code
     *   auto result = MeshCollisionBuilder::Build(verts, inds, transform);
     *   if (result.IsValid())
     *       physics->AddStaticMesh(result.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity());
     * @endcode
     *
     * @param shape    A validated Jolt RefConst<Shape> (e.g. from MeshShape::Create()).
     * @param position World-space origin of the body (usually RVec3::sZero() for scenes
     *                 where the transform is already baked into the shape vertices).
     * @param rotation World-space rotation quaternion.
     * @return PhysicsBodyHandle Handle to the created body, or an invalid handle on failure.
     */
    PhysicsBodyHandle AddStaticMesh(
        JPH::RefConst<JPH::Shape> shape,
        JPH::RVec3                position = JPH::RVec3::sZero(),
        JPH::Quat                 rotation = JPH::Quat::sIdentity());

    /**
     * @brief Adds a dynamic box linked to an entity.
     * @param entityID ID of the associated game entity.
     * @param position World position.
     * @param halfExtents Dimensions of the box.
     * @param restitution Bounciness factor.
     * @param linearDamping Movement damping factor.
     * @return PhysicsBodyHandle Handle to the created body.
     */
    PhysicsBodyHandle AddDynamicBox(
        uint32_t   entityID,
        JPH::RVec3 position,
        JPH::Vec3  halfExtents,
        float      restitution   = 0.3f,
        float      linearDamping = 0.05f);

    /**
     * @brief Adds a dynamic sphere linked to an entity.
     * @param entityID ID of the associated game entity.
     * @param position World position.
     * @param radius Radius of the sphere.
     * @param restitution Bounciness factor.
     * @return PhysicsBodyHandle Handle to the created body.
     */
    PhysicsBodyHandle AddDynamicSphere(
        uint32_t   entityID,
        JPH::RVec3 position,
        float      radius,
        float      restitution = 0.4f);

    /**
     * @brief Adds a kinematic box (moved by velocity, not forces).
     * @param entityID ID of the associated game entity.
     * @param position World position.
     * @param halfExtents Dimensions of the box.
     * @return PhysicsBodyHandle Handle to the created body.
     */
    PhysicsBodyHandle AddKinematicBox(
        uint32_t   entityID,
        JPH::RVec3 position,
        JPH::Vec3  halfExtents);

    /** @brief Removes and destroys a physics body from the world. */
    void RemoveBody(PhysicsBodyHandle handle);

    /** @brief Gets the current world position of a body. */
    JPH::RVec3 GetPosition      (PhysicsBodyHandle h) const;
    /** @brief Gets the current world rotation of a body. */
    JPH::Quat  GetRotation      (PhysicsBodyHandle h) const;
    /** @brief Gets the current linear velocity of a body. */
    JPH::Vec3  GetLinearVelocity(PhysicsBodyHandle h) const;

    /** @brief Sets the linear velocity of a body. */
    void SetLinearVelocity(PhysicsBodyHandle h, JPH::Vec3 vel);
    /** @brief Applies an instantaneous linear impulse to a body. */
    void AddImpulse       (PhysicsBodyHandle h, JPH::Vec3 impulse);
    /** @brief Instantly teleports a body to a new position and rotation. */
    void Teleport         (PhysicsBodyHandle h, JPH::RVec3 pos, JPH::Quat rot);

    /** @brief Checks if a body is currently in an active (simulating) state. */
    bool IsBodyActive(PhysicsBodyHandle h) const;

    /** @brief Returns a reference to the underlying Jolt physics system. */
    JPH::PhysicsSystem& GetSystem() { return *mPhysicsSystem; }

    /** @brief Gets the total number of simulation steps performed. */
    uint64_t GetStepCount()   const { return mStepCount; }
    /** @brief Gets the remaining time in the fixed-step accumulator. */
    float    GetAccumulator() const { return mAccumulator; }

private:
    /** @brief Helper to initialize Jolt broadphase filters and interfaces. */
    void BuildBroadphaseHelpers();
    /** @brief Scans tracked bodies and populates the snapshot list. */
    void CollectSnapshots();

    Config mCfg;

    std::unique_ptr<JPH::TempAllocatorImpl>    mTempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool>  mJobSystem;
    std::unique_ptr<JPH::PhysicsSystem>        mPhysicsSystem;

    std::unique_ptr<BugminBPLayerInterface>       mBPLayerInterface;
    std::unique_ptr<BugminObjVsBPLayerFilter>     mObjVsBPLayerFilter;
    std::unique_ptr<BugminObjLayerPairFilter>     mObjLayerPairFilter;
    std::unique_ptr<BugminContactListener>        mContactListener;
    std::unique_ptr<BugminBodyActivationListener> mBodyActivationListener;

    std::unordered_map<uint32_t, JPH::BodyID> mEntityToBody;
    std::unordered_map<uint32_t, uint32_t>    mBodyToEntity;

    std::vector<TransformSnapshot> mSnapshots;

    float    mAccumulator = 0.f;
    uint64_t mStepCount   = 0;
    bool     mInitialised = false;
};
