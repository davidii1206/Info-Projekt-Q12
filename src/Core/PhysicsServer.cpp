/**
 * @file PhysicsServer.cpp
 * @brief Implementation of the PhysicsServer using the Jolt Physics engine.
 */

#include "PhysicsServer.h"

// Jolt core
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

// Shapes
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

// Body creation
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

// Contact
#include <Jolt/Physics/Collision/ContactListener.h>

#include <iostream>
#include <thread>
#include <cassert>

JPH_SUPPRESS_WARNINGS
using namespace JPH;

/**
 * @namespace BPLayers
 * @brief Broadphase layer definitions.
 */
namespace BPLayers
{
    static constexpr BroadPhaseLayer NON_MOVING(0); ///< Static geometry.
    static constexpr BroadPhaseLayer MOVING    (1); ///< Moving objects.
    static constexpr uint32_t        COUNT      = 2; ///< Total number of broadphase layers.
}

/**
 * @class BugminBPLayerInterface
 * @brief Implementation of Jolt's BroadPhaseLayerInterface.
 * 
 * Maps ObjectLayers to BroadPhaseLayers.
 */
class BugminBPLayerInterface final : public BroadPhaseLayerInterface
{
public:
    BugminBPLayerInterface()
    {
        mMap[ObjectLayers::STATIC]  = BPLayers::NON_MOVING;
        mMap[ObjectLayers::DYNAMIC] = BPLayers::MOVING;
        mMap[ObjectLayers::TRIGGER] = BPLayers::MOVING; // triggers move with the world
    }

    uint32 GetNumBroadPhaseLayers() const override
    {
        return BPLayers::COUNT;
    }

    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        assert(inLayer < ObjectLayers::COUNT);
        return mMap[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
        case (BroadPhaseLayer::Type)BPLayers::NON_MOVING: return "NON_MOVING";
        case (BroadPhaseLayer::Type)BPLayers::MOVING:     return "MOVING";
        default:                                           return "UNKNOWN";
        }
    }
#endif

private:
    BroadPhaseLayer mMap[ObjectLayers::COUNT];
};

/**
 * @class BugminObjVsBPLayerFilter
 * @brief Filter to determine if an ObjectLayer should collide with a BroadPhaseLayer.
 */
class BugminObjVsBPLayerFilter final : public ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(ObjectLayer inObjLayer, BroadPhaseLayer inBPLayer) const override
    {
        switch (inObjLayer)
        {
        case ObjectLayers::STATIC:
            // Static bodies only need tests against moving bodies
            return inBPLayer == BPLayers::MOVING;

        case ObjectLayers::DYNAMIC:
        case ObjectLayers::TRIGGER:
            // Dynamic / triggers test against everything
            return true;

        default:
            assert(false && "Unknown ObjectLayer");
            return false;
        }
    }
};

/**
 * @class BugminObjLayerPairFilter
 * @brief Filter to determine if two ObjectLayers should collide.
 */
class BugminObjLayerPairFilter final : public ObjectLayerPairFilter
{
public:
    bool ShouldCollide(ObjectLayer inLayer1, ObjectLayer inLayer2) const override
    {
        /**
         * Collision matrix:
         * STATIC  vs STATIC  → no  (neither moves, pointless)
         * STATIC  vs DYNAMIC → yes
         * STATIC  vs TRIGGER → no  (triggers are handled via overlap queries)
         * DYNAMIC vs DYNAMIC → yes
         * DYNAMIC vs TRIGGER → yes (so we can detect entry)
         * TRIGGER vs TRIGGER → no
         */

        if (inLayer1 == ObjectLayers::STATIC && inLayer2 == ObjectLayers::STATIC)
            return false;
        if (inLayer1 == ObjectLayers::TRIGGER && inLayer2 == ObjectLayers::TRIGGER)
            return false;
        if (inLayer1 == ObjectLayers::STATIC  && inLayer2 == ObjectLayers::TRIGGER)
            return false;
        if (inLayer1 == ObjectLayers::TRIGGER && inLayer2 == ObjectLayers::STATIC)
            return false;

        return true;
    }
};

/**
 * @class BugminContactListener
 * @brief Listener for physics contact events.
 */
class BugminContactListener final : public ContactListener
{
public:
    ValidateResult OnContactValidate(
        const Body&,
        const Body&,
        RVec3Arg,
        const CollideShapeResult&) override
    {
        return ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(
        const Body&            b1,
        const Body&            b2,
        const ContactManifold&,
        ContactSettings&) override
    {
        // TODO: push (b1.GetID(), b2.GetID()) onto a lock-free queue;
        //       let the game thread read it to fire game events (damage, sounds…)
        (void)b1; (void)b2;
    }

    void OnContactPersisted(const Body&, const Body&, const ContactManifold&, ContactSettings&) override {}
    void OnContactRemoved(const SubShapeIDPair&) override {}
};

/**
 * @class BugminBodyActivationListener
 * @brief Listener for body activation/deactivation events.
 */
class BugminBodyActivationListener final : public BodyActivationListener
{
public:
    void OnBodyActivated  (const BodyID& id, uint64) override { (void)id; }
    void OnBodyDeactivated(const BodyID& id, uint64) override { (void)id; }
};

PhysicsServer::PhysicsServer(Config cfg)
    : mCfg(std::move(cfg))
{}

PhysicsServer::~PhysicsServer()
{
    if (mInitialised)
        Shutdown();
}

void PhysicsServer::Init()
{
    assert(!mInitialised && "PhysicsServer::Init called twice");

    /// 1. Memory allocator: Must be the very first Jolt call.
    RegisterDefaultAllocator();

    /// 2. Factory: Jolt uses a global factory to look up type info.
    Factory::sInstance = new Factory();

    /// 3. Register all built-in Jolt types.
    RegisterTypes();

    /// 4. Temp allocator: Scratchpad for per-frame allocations.
    mTempAllocator = std::make_unique<TempAllocatorImpl>(mCfg.tempAllocatorBytes);

    /// 5. Job system: Multi-threaded solver.
    const int threads = (mCfg.workerThreads < 0)
        ? std::max(1, (int)std::thread::hardware_concurrency() - 1)
        : mCfg.workerThreads;

    mJobSystem = std::make_unique<JobSystemThreadPool>(
        cMaxPhysicsJobs,
        cMaxPhysicsBarriers,
        threads
    );

    /// 6. Broadphase helpers.
    BuildBroadphaseHelpers();

    /// 7. Physics system initialization.
    mPhysicsSystem = std::make_unique<PhysicsSystem>();
    mPhysicsSystem->Init(
        mCfg.maxBodies,
        0,                          // mutex count: 0 = Jolt picks the best default
        mCfg.maxBodyPairs,
        mCfg.maxContactConstraints,
        *mBPLayerInterface,
        *mObjVsBPLayerFilter,
        *mObjLayerPairFilter
    );

    /// 8. Gravity & listeners.
    mPhysicsSystem->SetGravity(mCfg.gravity);

    mContactListener        = std::make_unique<BugminContactListener>();
    mBodyActivationListener = std::make_unique<BugminBodyActivationListener>();
    mPhysicsSystem->SetContactListener(mContactListener.get());
    mPhysicsSystem->SetBodyActivationListener(mBodyActivationListener.get());

    mInitialised = true;

    std::cout << "[PhysicsServer] Ready — "
              << threads << " worker thread(s), "
              << "timestep=" << mCfg.fixedTimestep << "s\n";
}

void PhysicsServer::BuildBroadphaseHelpers()
{
    mBPLayerInterface    = std::make_unique<BugminBPLayerInterface>();
    mObjVsBPLayerFilter  = std::make_unique<BugminObjVsBPLayerFilter>();
    mObjLayerPairFilter  = std::make_unique<BugminObjLayerPairFilter>();
}

const std::vector<TransformSnapshot>& PhysicsServer::Step(float deltaTime)
{
    assert(mInitialised && "Call Init() before Step()");

    mAccumulator += deltaTime;

    while (mAccumulator >= mCfg.fixedTimestep)
    {
        /// Core solver: broad phase → narrow phase → constraint solve → integrate.
        EPhysicsUpdateError err = mPhysicsSystem->Update(
            mCfg.fixedTimestep,
            mCfg.collisionSteps,
            mTempAllocator.get(),
            mJobSystem.get()
        );

        if (err != EPhysicsUpdateError::None)
        {
            std::cerr << "[PhysicsServer] Update error flags: "
                      << static_cast<uint32_t>(err) << "\n";
        }

        mAccumulator -= mCfg.fixedTimestep;
        ++mStepCount;
    }

    /// Collect transforms of all tracked dynamic bodies.
    CollectSnapshots();
    return mSnapshots;
}

void PhysicsServer::CollectSnapshots()
{
    mSnapshots.clear();

    /// Use the no-lock interface for reading snapshots.
    const BodyInterface& bi = mPhysicsSystem->GetBodyInterfaceNoLock();

    for (auto& [entityID, bodyID] : mEntityToBody)
    {
        if (!bi.IsAdded(bodyID))
            continue;

        TransformSnapshot snap;
        snap.entityID   = entityID;
        snap.position   = bi.GetCenterOfMassPosition(bodyID);
        snap.rotation   = bi.GetRotation(bodyID);
        snap.linearVel  = bi.GetLinearVelocity(bodyID);
        snap.angularVel = bi.GetAngularVelocity(bodyID);
        mSnapshots.push_back(snap);
    }
}

void PhysicsServer::Shutdown()
{
    assert(mInitialised && "PhysicsServer::Shutdown called without Init");

    BodyInterface& bi = mPhysicsSystem->GetBodyInterface();

    /// Remove and destroy every tracked body.
    for (auto& [entityID, bodyID] : mEntityToBody)
    {
        if (bi.IsAdded(bodyID))
            bi.RemoveBody(bodyID);
        bi.DestroyBody(bodyID);
    }
    mEntityToBody.clear();
    mBodyToEntity.clear();

    /// Destroy the system first (may reference allocators/job system).
    mPhysicsSystem.reset();

    mContactListener.reset();
    mBodyActivationListener.reset();
    mBPLayerInterface.reset();
    mObjVsBPLayerFilter.reset();
    mObjLayerPairFilter.reset();
    mJobSystem.reset();
    mTempAllocator.reset();

    /// Jolt global teardown.
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    mInitialised = false;

    std::cout << "[PhysicsServer] Shutdown — "
              << mStepCount << " steps simulated.\n";
}

PhysicsBodyHandle PhysicsServer::AddStaticFloor(RVec3 centre, float halfExtentX, float halfExtentZ)
{
    return AddStaticBox(
        RVec3(centre.GetX(), centre.GetY() - 0.5f, centre.GetZ()),
        Vec3(halfExtentX, 0.5f, halfExtentZ)
    );
}

PhysicsBodyHandle PhysicsServer::AddStaticBox(RVec3 position, Vec3 halfExtents, Quat rotation)
{
    BodyInterface& bi = mPhysicsSystem->GetBodyInterface();

    BoxShapeSettings shapeSettings(halfExtents);
    shapeSettings.mConvexRadius = 0.01f;

    auto shapeResult = shapeSettings.Create();
    if (!shapeResult.IsValid())
    {
        std::cerr << "[PhysicsServer] AddStaticBox: shape creation failed\n";
        return {};
    }

    BodyCreationSettings bcs(
        shapeResult.Get(),
        position,
        rotation,
        EMotionType::Static,
        ObjectLayers::STATIC
    );

    BodyID id = bi.CreateAndAddBody(bcs, EActivation::DontActivate);
    return PhysicsBodyHandle{ id };
}

PhysicsBodyHandle PhysicsServer::AddDynamicBox(
    uint32_t entityID, RVec3 position, Vec3 halfExtents,
    float restitution, float linearDamping)
{
    BodyInterface& bi = mPhysicsSystem->GetBodyInterface();

    BoxShapeSettings shapeSettings(halfExtents);
    auto shapeResult = shapeSettings.Create();
    if (!shapeResult.IsValid())
    {
        std::cerr << "[PhysicsServer] AddDynamicBox: shape creation failed\n";
        return {};
    }

    BodyCreationSettings bcs(
        shapeResult.Get(),
        position,
        Quat::sIdentity(),
        EMotionType::Dynamic,
        ObjectLayers::DYNAMIC
    );
    bcs.mRestitution   = restitution;
    bcs.mLinearDamping = linearDamping;
    bcs.mUserData      = static_cast<uint64>(entityID); // store entityID in body

    BodyID id = bi.CreateAndAddBody(bcs, EActivation::Activate);
    if (!id.IsInvalid())
    {
        mEntityToBody[entityID] = id;
        mBodyToEntity[id.GetIndex()] = entityID;
    }
    return PhysicsBodyHandle{ id };
}

PhysicsBodyHandle PhysicsServer::AddDynamicSphere(
    uint32_t entityID, RVec3 position, float radius, float restitution)
{
    BodyInterface& bi = mPhysicsSystem->GetBodyInterface();

    SphereShapeSettings shapeSettings(radius);
    auto shapeResult = shapeSettings.Create();
    if (!shapeResult.IsValid())
    {
        std::cerr << "[PhysicsServer] AddDynamicSphere: shape creation failed\n";
        return {};
    }

    BodyCreationSettings bcs(
        shapeResult.Get(),
        position,
        Quat::sIdentity(),
        EMotionType::Dynamic,
        ObjectLayers::DYNAMIC
    );
    bcs.mRestitution = restitution;
    bcs.mUserData    = static_cast<uint64>(entityID);

    BodyID id = bi.CreateAndAddBody(bcs, EActivation::Activate);
    if (!id.IsInvalid())
    {
        mEntityToBody[entityID] = id;
        mBodyToEntity[id.GetIndex()] = entityID;
    }
    return PhysicsBodyHandle{ id };
}

PhysicsBodyHandle PhysicsServer::AddKinematicBox(
    uint32_t entityID, RVec3 position, Vec3 halfExtents)
{
    BodyInterface& bi = mPhysicsSystem->GetBodyInterface();

    BoxShapeSettings shapeSettings(halfExtents);
    auto shapeResult = shapeSettings.Create();
    if (!shapeResult.IsValid())
    {
        std::cerr << "[PhysicsServer] AddKinematicBox: shape creation failed\n";
        return {};
    }

    BodyCreationSettings bcs(
        shapeResult.Get(),
        position,
        Quat::sIdentity(),
        EMotionType::Kinematic,  // moved by velocity, not forces
        ObjectLayers::DYNAMIC
    );
    bcs.mUserData = static_cast<uint64>(entityID);

    BodyID id = bi.CreateAndAddBody(bcs, EActivation::Activate);
    if (!id.IsInvalid())
    {
        mEntityToBody[entityID] = id;
        mBodyToEntity[id.GetIndex()] = entityID;
    }
    return PhysicsBodyHandle{ id };
}

void PhysicsServer::RemoveBody(PhysicsBodyHandle handle)
{
    if (!handle.IsValid()) return;

    BodyInterface& bi = mPhysicsSystem->GetBodyInterface();
    if (bi.IsAdded(handle.id))
        bi.RemoveBody(handle.id);
    bi.DestroyBody(handle.id);

    /// Clean up entity-to-body maps.
    auto bodyIt = mBodyToEntity.find(handle.id.GetIndex());
    if (bodyIt != mBodyToEntity.end())
    {
        mEntityToBody.erase(bodyIt->second);
        mBodyToEntity.erase(bodyIt);
    }
}

RVec3 PhysicsServer::GetPosition(PhysicsBodyHandle h) const
{
    return mPhysicsSystem->GetBodyInterfaceNoLock().GetCenterOfMassPosition(h.id);
}

Quat PhysicsServer::GetRotation(PhysicsBodyHandle h) const
{
    return mPhysicsSystem->GetBodyInterfaceNoLock().GetRotation(h.id);
}

Vec3 PhysicsServer::GetLinearVelocity(PhysicsBodyHandle h) const
{
    return mPhysicsSystem->GetBodyInterfaceNoLock().GetLinearVelocity(h.id);
}

void PhysicsServer::SetLinearVelocity(PhysicsBodyHandle h, Vec3 vel)
{
    mPhysicsSystem->GetBodyInterface().SetLinearVelocity(h.id, vel);
}

void PhysicsServer::AddImpulse(PhysicsBodyHandle h, Vec3 impulse)
{
    mPhysicsSystem->GetBodyInterface().AddImpulse(h.id, impulse);
}

void PhysicsServer::Teleport(PhysicsBodyHandle h, RVec3 pos, Quat rot)
{
    mPhysicsSystem->GetBodyInterface().SetPositionAndRotation(
        h.id, pos, rot, EActivation::Activate
    );
}

bool PhysicsServer::IsBodyActive(PhysicsBodyHandle h) const
{
    return mPhysicsSystem->GetBodyInterfaceNoLock().IsActive(h.id);
}
