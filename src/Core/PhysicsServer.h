#pragma once

// =============================================================================
// src/Core/PhysicsServer.h
// Server-side Jolt Physics integration for bugmin
// =============================================================================

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>

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

// ---------------------------------------------------------------------------
// Object layers
// ---------------------------------------------------------------------------
namespace ObjectLayers
{
    inline constexpr JPH::ObjectLayer STATIC   = 0;
    inline constexpr JPH::ObjectLayer DYNAMIC  = 1;
    inline constexpr JPH::ObjectLayer TRIGGER  = 2;
    inline constexpr uint32_t         COUNT    = 3;
}

// ---------------------------------------------------------------------------
// PhysicsBodyHandle — thin wrapper so game code never touches raw BodyIDs
// ---------------------------------------------------------------------------
struct PhysicsBodyHandle
{
    JPH::BodyID id;
    bool IsValid() const { return !id.IsInvalid(); }
    bool operator==(const PhysicsBodyHandle& o) const { return id == o.id; }
};

// ---------------------------------------------------------------------------
// TransformSnapshot — what the server broadcasts to clients each tick
// ---------------------------------------------------------------------------
struct TransformSnapshot
{
    uint32_t   entityID   = 0;
    JPH::RVec3 position   = JPH::RVec3::sZero();
    JPH::Quat  rotation   = JPH::Quat::sIdentity();
    JPH::Vec3  linearVel  = JPH::Vec3::sZero();
    JPH::Vec3  angularVel = JPH::Vec3::sZero();
};

// ---------------------------------------------------------------------------
// PhysicsServerConfig — defined OUTSIDE the class so Config{} works as a
// C++17 default parameter without triggering the "incomplete type" error.
// ---------------------------------------------------------------------------
struct PhysicsServerConfig
{
    float    fixedTimestep         = 1.0f / 60.0f;
    int      collisionSteps        = 1;
    uint32_t maxBodies             = 4096;
    uint32_t maxBodyPairs          = 65536;
    uint32_t maxContactConstraints = 16384;
    uint32_t tempAllocatorBytes    = 10u * 1024u * 1024u; // 10 MB
    int      workerThreads         = -1;                  // -1 = auto
    JPH::Vec3 gravity              = JPH::Vec3(0.f, -9.81f, 0.f);
};

// ---------------------------------------------------------------------------
// PhysicsServer
// ---------------------------------------------------------------------------
class PhysicsServer
{
public:
    using Config = PhysicsServerConfig;

    explicit PhysicsServer(Config cfg = Config{});
    ~PhysicsServer();

    PhysicsServer(const PhysicsServer&)            = delete;
    PhysicsServer& operator=(const PhysicsServer&) = delete;

    // Lifecycle
    void Init();
    void Shutdown();

    // Fixed-step update — returns snapshots to broadcast
    const std::vector<TransformSnapshot>& Step(float deltaTime);

    // Body factory
    PhysicsBodyHandle AddStaticFloor(
        JPH::RVec3 centre     = JPH::RVec3(0, 0, 0),
        float halfExtentX = 100.f, float halfExtentZ = 100.f);

    PhysicsBodyHandle AddStaticBox(
        JPH::RVec3 position,
        JPH::Vec3  halfExtents,
        JPH::Quat  rotation = JPH::Quat::sIdentity());

    PhysicsBodyHandle AddDynamicBox(
        uint32_t   entityID,
        JPH::RVec3 position,
        JPH::Vec3  halfExtents,
        float      restitution   = 0.3f,
        float      linearDamping = 0.05f);

    PhysicsBodyHandle AddDynamicSphere(
        uint32_t   entityID,
        JPH::RVec3 position,
        float      radius,
        float      restitution = 0.4f);

    PhysicsBodyHandle AddKinematicBox(
        uint32_t   entityID,
        JPH::RVec3 position,
        JPH::Vec3  halfExtents);

    void RemoveBody(PhysicsBodyHandle handle);

    // Per-body queries / setters
    JPH::RVec3 GetPosition      (PhysicsBodyHandle h) const;
    JPH::Quat  GetRotation      (PhysicsBodyHandle h) const;
    JPH::Vec3  GetLinearVelocity(PhysicsBodyHandle h) const;

    void SetLinearVelocity(PhysicsBodyHandle h, JPH::Vec3 vel);
    void AddImpulse       (PhysicsBodyHandle h, JPH::Vec3 impulse);
    void Teleport         (PhysicsBodyHandle h, JPH::RVec3 pos, JPH::Quat rot);

    bool IsBodyActive(PhysicsBodyHandle h) const;

    JPH::PhysicsSystem& GetSystem() { return *mPhysicsSystem; }

    uint64_t GetStepCount()   const { return mStepCount; }
    float    GetAccumulator() const { return mAccumulator; }

private:
    void BuildBroadphaseHelpers();
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
