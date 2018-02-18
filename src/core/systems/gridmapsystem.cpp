#include "gridmap.hpp"
#include "core/systems/gridmapsystem.hpp"
#include "core/systems/camerasystem.hpp"
#include "core/entity.hpp"
#include "core/components/transformcomponent.hpp"
#include "resourcemapper.hpp"
#include "oddlib/bits_factory.hpp"

DEFINE_SYSTEM(GridmapSystem);

GridmapSystem::GridmapSystem(CoordinateSpace& coords) 
    : mCoords(coords)
{

}

void GridmapSystem::OnLoad()
{
    mGridMap = std::make_unique<GridMap>(mCoords, *mManager);
}

void GridmapSystem::MoveToCamera(ResourceLocator& locator, u32 xIndex, u32 yIndex)
{
    CameraSystem* cameraSystem = mManager->GetSystem<CameraSystem>();

    GridScreenData* pData = mGridMap->GetGridScreen(xIndex, yIndex);
    assert(pData);

    Entity entity = mManager->CreateEntityWith<GridMapScreenComponent, TransformComponent>();

    GridMapScreenComponent* gridMapScreen = entity.GetComponent<GridMapScreenComponent>();
    gridMapScreen->LoadCamera(locator, pData->mCameraAndObjects.mName);

    TransformComponent* pTransform = entity.GetComponent<TransformComponent>();
    pTransform->Set(xIndex * cameraSystem->mCameraBlockSize.x, yIndex * cameraSystem->mCameraBlockSize.y);
}

void GridmapSystem::MoveToCamera(const char* /*cameraName*/)
{
    mManager->CreateEntityWith<GridMapScreenComponent, TransformComponent>();
}

bool GridmapSystem::LoadMap(const PathInformation& pathInfo)
{
    return mGridMap->LoadMap(pathInfo);
}

void GridmapSystem::UnloadMap(AbstractRenderer& renderer) const
{
    return mGridMap->UnloadMap(renderer);
}

DEFINE_COMPONENT(GridMapScreenComponent);

GridMapScreenComponent::GridMapScreenComponent()
{

}

void GridMapScreenComponent::Render(AbstractRenderer& rend, float x, float y, float w, float h) const
{
    if (mBits)
    {
        SDL_Surface* pBackgroundImage = mBits->GetSurface();
        if (pBackgroundImage)
        {
            TextureHandle backgroundText = rend.CreateTexture(AbstractRenderer::eTextureFormats::eRGB, 
                static_cast<u32>(pBackgroundImage->w),
                static_cast<u32>(pBackgroundImage->h),
                AbstractRenderer::eTextureFormats::eRGB,
                pBackgroundImage->pixels, true);

            rend.TexturedQuad(backgroundText, x, y, w, h, AbstractRenderer::eForegroundLayer0, ColourU8{ 255, 255, 255, 255 });
        }
    }

    //mBits->Render(rend, x, y ,w ,h);
}

void GridMapScreenComponent::LoadCamera(ResourceLocator& locator, const std::string& name)
{
    mBits = locator.LocateCamera(name).get();
}
