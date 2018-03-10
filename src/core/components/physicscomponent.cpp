#include <ostream>
#include <istream>

#include "core/entity.hpp"
#include "core/components/physicscomponent.hpp"

DEFINE_COMPONENT(PhysicsComponent);

void PhysicsComponent::Serialize(std::ostream& os) const
{
    static_assert(std::is_pod<decltype(mData)>::value, "PhysicsComponent::mData is not a POD type");
    os.write(static_cast<const char*>(static_cast<const void*>(&mData)), sizeof(decltype(mData)));
}

void PhysicsComponent::Deserialize(std::istream& is)
{
    static_assert(std::is_pod<decltype(mData)>::value, "PhysicsComponent::mData is not a POD type");
    is.read(static_cast<char*>(static_cast<void*>(&mData)), sizeof(decltype(mData)));
}

float PhysicsComponent::GetXSpeed() const
{
    return mData.mXSpeed;
}

float PhysicsComponent::GetYSpeed() const
{
    return mData.mYSpeed;
}

float PhysicsComponent::GetXVelocity() const
{
    return mData.mXVelocity;
}

float PhysicsComponent::GetYVelocity() const
{
    return mData.mYVelocity;
}

void PhysicsComponent::SetSpeed(float xSpeed, float ySpeed)
{
    mData.mXSpeed = xSpeed;
    mData.mYSpeed = ySpeed;
}

void PhysicsComponent::SetXSpeed(float xSpeed)
{
    mData.mXSpeed = xSpeed;
}

void PhysicsComponent::SetYSpeed(float ySpeed)
{
    mData.mYSpeed = ySpeed;
}

void PhysicsComponent::SetVelocity(float velocityX, float velocityY)
{
    mData.mXVelocity = velocityX;
    mData.mYVelocity = velocityY;
}

void PhysicsComponent::SetXVelocity(float velocityX)
{
    mData.mXVelocity = velocityX;
}

void PhysicsComponent::SetYVelocity(float velocityY)
{
    mData.mYVelocity = velocityY;
}
