#pragma once

#include "core/component.hpp"

class TransformComponent;

class PhysicsComponent final : public Component
{
public:
    DECLARE_COMPONENT(PhysicsComponent);

public:
    void Serialize(std::ostream& os) const final;
    void Deserialize(std::istream& is) final;

public:
    float GetXSpeed() const;
    float GetYSpeed() const;
    float GetXVelocity() const;
    float GetYVelocity() const;
    void SetSpeed(float xSpeed, float ySpeed);
    void SetXSpeed(float xSpeed);
    void SetYSpeed(float ySpeed);
    void SetVelocity(float velocityX, float velocityY);
    void SetXVelocity(float velocityX);
    void SetYVelocity(float velocityY);

private:
    struct
    {
        float mXSpeed;
        float mYSpeed;
        float mXVelocity;
        float mYVelocity;
    } mData = {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };
};
