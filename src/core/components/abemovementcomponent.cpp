#include "core/entity.hpp"
#include "core/entitymanager.hpp"
#include "core/systems/inputsystem.hpp"
#include "core/components/physicscomponent.hpp"
#include "core/components/transformcomponent.hpp"
#include "core/components/animationcomponent.hpp"
#include "core/components/abemovementcomponent.hpp"
#include "core/components/sligmovementcomponent.hpp"

DEFINE_COMPONENT(AbeMovementComponent);

static const std::string kAbeWalkToStand = "AbeWalkToStand";
static const std::string kAbeWalkToStandMidGrid = "AbeWalkToStandMidGrid";
static const std::string kAbeWalkingToRunning = "AbeWalkingToRunning";
static const std::string kAbeWalkingToRunningMidGrid = "AbeWalkingToRunningMidGrid";
static const std::string kAbeWalkingToSneaking = "AbeWalkingToSneaking";
static const std::string kAbeWalkingToSneakingMidGrid = "AbeWalkingToSneakingMidGrid";
static const std::string kAbeStandToRun = "AbeStandToRun";
static const std::string kAbeRunningToSkidTurn = "AbeRunningToSkidTurn";
static const std::string kAbeRunningTurnAround = "AbeRunningTurnAround";
static const std::string kAbeRunningTurnAroundToWalk = "AbeRunningTurnAroundToWalk";
static const std::string kAbeRunningToRoll = "AbeRunningToRoll";
static const std::string kAbeRuningToJump = "AbeRuningToJump";
static const std::string kAbeRunningJumpInAir = "AbeRunningJumpInAir";
static const std::string kAbeLandToRunning = "AbeLandToRunning";
static const std::string kAbeLandToWalking = "AbeLandToWalking";
static const std::string kAbeFallingToLand = "AbeFallingToLand";
static const std::string kRunToSkidStop = "RunToSkidStop";
static const std::string kAbeRunningSkidStop = "AbeRunningSkidStop";
static const std::string kAbeRunningToWalk = "AbeRunningToWalk";
static const std::string kAbeRunningToWalkingMidGrid = "AbeRunningToWalkingMidGrid";
static const std::string kAbeStandToSneak = "AbeStandToSneak";
static const std::string kAbeSneakToStand = "AbeSneakToStand";
static const std::string kAbeSneakToStandMidGrid = "AbeSneakToStandMidGrid";
static const std::string kAbeSneakingToWalking = "AbeSneakingToWalking";
static const std::string kAbeSneakingToWalkingMidGrid = "AbeSneakingToWalkingMidGrid";
static const std::string kAbeStandPushWall = "AbeStandPushWall";
static const std::string kAbeHitGroundToStand = "AbeHitGroundToStand";
static const std::string kAbeStandToWalk = "AbeStandToWalk";
static const std::string kAbeStandToCrouch = "AbeStandToCrouch";
static const std::string kAbeCrouchToStand = "AbeCrouchToStand";
static const std::string kAbeStandTurnAround = "AbeStandTurnAround";
static const std::string kAbeStandTurnAroundToRunning = "AbeStandTurnAroundToRunning";
static const std::string kAbeCrouchTurnAround = "AbeCrouchTurnAround";
static const std::string kAbeCrouchToRoll = "AbeCrouchToRoll";
static const std::string kAbeStandSpeak1 = "AbeStandSpeak1";
static const std::string kAbeStandSpeak2 = "AbeStandSpeak2";
static const std::string kAbeStandSpeak3 = "AbeStandSpeak3";
static const std::string kAbeStandingSpeak4 = "AbeStandingSpeak4";
static const std::string kAbeStandSpeak5 = "AbeStandSpeak5";
static const std::string kAbeCrouchSpeak1 = "AbeCrouchSpeak1";
static const std::string kAbeCrouchSpeak2 = "AbeCrouchSpeak2";
static const std::string kAbeStandIdle = "AbeStandIdle";
static const std::string kAbeCrouchIdle = "AbeCrouchIdle";
static const std::string kAbeStandToHop = "AbeStandToHop";
static const std::string kAbeHopping = "AbeHopping";
static const std::string kAbeHoppingToStand = "AbeHoppingToStand";
static const std::string kAbeHoistDangling = "AbeHoistDangling";
static const std::string kAbeHoistPullSelfUp = "AbeHoistPullSelfUp";
static const std::string kAbeStandToJump = "AbeStandToJump";
static const std::string kAbeJumpUpFalling = "AbeJumpUpFalling";
static const std::string kAbeWalking = "AbeWalking";
static const std::string kAbeRunning = "AbeRunning";
static const std::string kAbeSneaking = "AbeSneaking";
static const std::string kAbeStandToFallingFromTrapDoor = "AbeStandToFallingFromTrapDoor";
static const std::string kAbeHoistDropDown = "AbeHoistDropDown";
static const std::string kAbeRolling = "AbeRolling";
static const std::string kAbeStandToChant = "AbeStandToChant";
static const std::string kAbeChantToStand = "AbeChantToStand";

void AbeMovementComponent::Deserialize(std::istream&)
{
    Load();
}

void AbeMovementComponent::Load()
{
    mPhysicsComponent = mEntity->GetComponent<PhysicsComponent>();
    mAnimationComponent = mEntity->GetComponent<AnimationComponent>();
    mTransformComponent = mEntity->GetComponent<TransformComponent>();

    mStateFnMap[States::eStanding] =            { &AbeMovementComponent::PreStanding,  &AbeMovementComponent::Standing          };
    mStateFnMap[States::eChanting] =            { &AbeMovementComponent::PreChanting,  &AbeMovementComponent::Chanting          };
    mStateFnMap[States::eWalking] =             { &AbeMovementComponent::PreWalking,   &AbeMovementComponent::Walking           };
    mStateFnMap[States::eStandTurningAround] =  { nullptr,                             &AbeMovementComponent::StandTurnAround   };

	SetAnimation(kAbeStandIdle);
}

void AbeMovementComponent::Update()
{
    auto it = mStateFnMap.find(mState);
    if (it != std::end(mStateFnMap) && it->second.mHandler)
    {
        it->second.mHandler(this);
    }
    else
    {
        ASyncTransition();
    }
}

void AbeMovementComponent::ASyncTransition()
{
    if (mAnimationComponent->Complete())
    {
        SetState(mNextState);
    }
}

bool AbeMovementComponent::DirectionChanged() const
{
    return (!mAnimationComponent->mFlipX && mGoal == Goal::eGoLeft) || (mAnimationComponent->mFlipX && mGoal == Goal::eGoRight);
}

bool AbeMovementComponent::TryMoveLeftOrRight() const
{
    return mGoal == Goal::eGoLeft || mGoal == Goal::eGoRight;
}

void AbeMovementComponent::SetAnimation(const std::string& anim)
{
    mAnimationComponent->Change(anim.c_str());
}

void AbeMovementComponent::SetState(AbeMovementComponent::States state)
{
    auto prevState = mState;
    mState = state;
    auto it = mStateFnMap.find(mState);
    if (it != std::end(mStateFnMap))
    {
        if (it->second.mPreHandler)
        {
            it->second.mPreHandler(this, prevState);
        }
    }
}

void AbeMovementComponent::PreStanding(AbeMovementComponent::States /*previous*/)
{
    SetAnimation(kAbeStandIdle);
    mPhysicsComponent->xSpeed = 0.0f;
    mPhysicsComponent->ySpeed = 0.0f;
}

void AbeMovementComponent::Standing()
{
    if (TryMoveLeftOrRight())
    {
        if (DirectionChanged())
        {
            SetAnimation(kAbeStandTurnAround);
            SetState(States::eStandTurningAround);
            mNextState = States::eStanding;
        }
        else
        {
            SetAnimation(kAbeStandToWalk);
            mNextState = States::eWalking;
            SetXSpeed(kAbeWalkSpeed);
            SetState(States::eStandToWalking);
        }
    }
    else if (mGoal == Goal::eChant)
    {
        SetState(States::eChanting);
    }
}

void AbeMovementComponent::PreChanting(AbeMovementComponent::States /*previous*/)
{
    SetAnimation(kAbeStandToChant);
}

void AbeMovementComponent::Chanting()
{
    if (mGoal == Goal::eStand)
    {
        SetAnimation(kAbeChantToStand);
        mNextState = States::eStanding;
        SetState(States::eChantToStand);
    }
    // Still chanting?
    else if (mGoal == Goal::eChant)
    {
        auto sligs = mEntity->GetManager()->With<SligMovementComponent>();
        if (!sligs.empty())
        {
            for (auto &slig : sligs)
            {
                LOG_INFO("Found a Slig to possess");
                slig->Destroy();
            }
        }
    }
}

void AbeMovementComponent::PreWalking(AbeMovementComponent::States /*previous*/)
{
    SetAnimation(kAbeWalking);
    SetXSpeed(kAbeWalkSpeed);
}

void AbeMovementComponent::Walking()
{
    if (mAnimationComponent->FrameNumber() == 5 + 1 || mAnimationComponent->FrameNumber() == 14 + 1)
    {
        mTransformComponent->SnapXToGrid();
    }

    if (DirectionChanged() || !TryMoveLeftOrRight())
    {
        if (mAnimationComponent->FrameNumber() == 2 + 1 || mAnimationComponent->FrameNumber() == 11 + 1)
        {
            SetState(States::eWalkingToStanding);
            mNextState = States::eStanding;
            SetAnimation(mAnimationComponent->FrameNumber() == 2 + 1 ? kAbeWalkToStand : kAbeWalkToStandMidGrid);
        }
    }
}

void AbeMovementComponent::StandTurnAround()
{
    if (mAnimationComponent->Complete())
    {
        mAnimationComponent->mFlipX = !mAnimationComponent->mFlipX;
        SetState(States::eStanding);
    }
}

void AbeMovementComponent::SetXSpeed(f32 speed)
{
    if (mAnimationComponent->mFlipX)
    {
        mPhysicsComponent->xSpeed = -speed;
    }
    else
    {
        mPhysicsComponent->xSpeed = speed;
    }
}

DEFINE_COMPONENT(AbePlayerControllerComponent);

void AbePlayerControllerComponent::Deserialize(std::istream&)
{
    Load();
}

void AbePlayerControllerComponent::Load()
{

    mEntity->GetManager()->With<InputSystem>([this](auto, auto inputSystem)
                                             {
                                                 mInputMappingActions = inputSystem->GetActions();
                                             });
    mAbeMovement = mEntity->GetComponent<AbeMovementComponent>();
}

void AbePlayerControllerComponent::Update()
{
    if (mInputMappingActions->Left(mInputMappingActions->mIsDown) && !mInputMappingActions->Right(mInputMappingActions->mIsDown))
    {
        mAbeMovement->mGoal = AbeMovementComponent::Goal::eGoLeft;
    }
    else if (mInputMappingActions->Right(mInputMappingActions->mIsDown) && !mInputMappingActions->Left(mInputMappingActions->mIsDown))
    {
        mAbeMovement->mGoal = AbeMovementComponent::Goal::eGoRight;
    }
    else if (mInputMappingActions->Chant(mInputMappingActions->mIsDown))
    {
        mAbeMovement->mGoal = AbeMovementComponent::Goal::eChant;
    }
    else
    {
        mAbeMovement->mGoal = AbeMovementComponent::Goal::eStand;
    }
}
