#include "gridmap.hpp"
#include "core/entity.hpp"
#include "core/systems/gridmapsystem.hpp"

DEFINE_SYSTEM(GridmapSystem);

GridmapSystem::GridmapSystem(CoordinateSpace& coords) : mCoords(coords)
{

}

void GridmapSystem::OnLoad()
{
    mGridMap = std::make_unique<GridMap>(mCoords, *mManager);
}

void GridmapSystem::MoveToCamera(const char* /*cameraName*/)
{
    mManager->CreateEntityWith<GridMapScreenComponent>();
}

bool GridmapSystem::LoadMap(const PathInformation& pathInfo, ResourceLocator& mLocator)
{
    return mGridMap->LoadMap(pathInfo, mLocator);
}

void GridmapSystem::UnloadMap(AbstractRenderer& renderer) const
{
    return mGridMap->UnloadMap(renderer);
}

DEFINE_COMPONENT(GridMapScreenComponent);

void GridMapScreenComponent::Render(AbstractRenderer& /*rend*/) const
{
    //mScreen->Render(rend);
}
