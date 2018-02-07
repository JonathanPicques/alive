#pragma once

#include "engine.hpp"

class Animation;

class AnimationBrowser
{
public:
    AnimationBrowser(AnimationBrowser&&) = delete;
    AnimationBrowser& operator = (AnimationBrowser&&) = delete;
    AnimationBrowser(ResourceLocator& resMapper);

    void Render(AbstractRenderer& renderer);
    void Update(const InputReader& input, CoordinateSpace& coords);
private:
    void RenderAnimationSelector(CoordinateSpace& coords);

    ResourceLocator& mResourceLocator;
    std::vector<std::unique_ptr<Animation>> mLoadedAnims;

    // Not owned
    Animation* mSelected = nullptr;

    s32 mXDelta = 0;
    s32 mYDelta = 0;

    bool mDebugResetAnimStates = false;
};
