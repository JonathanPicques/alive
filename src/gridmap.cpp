#include "gridmap.hpp"
#include "gui.h"
#include "oddlib/lvlarchive.hpp"
#include "renderer.hpp"
#include "oddlib/path.hpp"
#include "oddlib/bits_factory.hpp"
#include "logger.hpp"
#include <cassert>
#include "sdl_raii.hpp"
#include <algorithm> // min/max
#include <cmath>
#include "resourcemapper.hpp"
#include "engine.hpp"

namespace Physics
{
    bool raycast_lines(const glm::vec2& line1p1, const glm::vec2& line1p2, const glm::vec2& line2p1, const glm::vec2& line2p2, raycast_collision * collision)
    {
        //bool lines_intersect = false;
        bool segments_intersect = false;
        glm::vec2 intersection;
        glm::vec2 close_p1;
        glm::vec2 close_p2;

        // Get the segments' parameters.
        float dx12 = line1p2.x - line1p1.x;
        float dy12 = line1p2.y - line1p1.y;
        float dx34 = line2p2.x - line2p1.x;
        float dy34 = line2p2.y - line2p1.y;

        // Solve for t1 and t2
        float denominator = (dy12 * dx34 - dx12 * dy34);

        float t1 =
            ((line1p1.x - line2p1.x) * dy34 + (line2p1.y - line1p1.y) * dx34)
            / denominator;
        if (glm::isinf(t1))
        {
            // The lines are parallel (or close enough to it).
            //lines_intersect = false;
            segments_intersect = false;
            const float fnan = nanf("");
            intersection = glm::vec2(fnan, fnan);
            close_p1 = glm::vec2(fnan, fnan);
            close_p2 = glm::vec2(fnan, fnan);
            if (collision)
                collision->intersection = intersection;

            return segments_intersect;
        }
        //lines_intersect = true;

        float t2 =
            ((line2p1.x - line1p1.x) * dy12 + (line1p1.y - line2p1.y) * dx12)
            / -denominator;

        // Find the point of intersection.
        intersection = glm::vec2(line1p1.x + dx12 * t1, line1p1.y + dy12 * t1);
        if (collision)
            collision->intersection = intersection;

        // The segments intersect if t1 and t2 are between 0 and 1.
        segments_intersect =
            ((t1 >= 0) && (t1 <= 1) &&
            (t2 >= 0) && (t2 <= 1));

        // Find the closest points on the segments.
        if (t1 < 0)
        {
            t1 = 0;
        }
        else if (t1 > 1)
        {
            t1 = 1;
        }

        if (t2 < 0)
        {
            t2 = 0;
        }
        else if (t2 > 1)
        {
            t2 = 1;
        }

        close_p1 = glm::vec2(line1p1.x + dx12 * t1, line1p1.y + dy12 * t1);
        close_p2 = glm::vec2(line2p1.x + dx34 * t2, line2p1.y + dy34 * t2);

        return segments_intersect;
    }
}

MapObject::MapObject(IMap& map, sol::state& luaState, ResourceLocator& locator, const ObjRect& rect)
    : mMap(map), mLuaState(luaState), mLocator(locator), mRect(rect)
{

}

MapObject::MapObject(IMap& map, sol::state& luaState, ResourceLocator& locator, const std::string& scriptName)
    : mMap(map), mLuaState(luaState), mLocator(locator), mScriptName(scriptName)
{

}

/*static*/ void MapObject::RegisterLuaBindings(sol::state& state)
{
    state.new_usertype<MapObject>("MapObject",
        "SetAnimation", &MapObject::SetAnimation,
        "SetAnimationFrame", &MapObject::SetAnimationFrame,
        "FrameNumber", &MapObject::FrameNumber,
        "IsLastFrame", &MapObject::IsLastFrame,
        "AnimUpdate", &MapObject::AnimUpdate,
        "SetAnimationAtFrame", &MapObject::SetAnimationAtFrame,
        "AnimationComplete", &MapObject::AnimationComplete,
        "NumberOfFrames", &MapObject::NumberOfFrames,
        "FrameCounter", &MapObject::FrameCounter,

        "WallCollision", &MapObject::WallCollision,
        "CellingCollision", &MapObject::CellingCollision,
        "FloorCollision", &MapObject::FloorCollision,

        "SnapXToGrid", &MapObject::SnapXToGrid,
        "FacingLeft", &MapObject::FacingLeft,

        "FacingRight", &MapObject::FacingRight,
        "FlipXDirection", &MapObject::FlipXDirection,
        "states", &MapObject::mStates,
        "mXPos", &MapObject::mXPos,
        "mYPos", &MapObject::mYPos);
}

void MapObject::Init()
{
    LoadScript();
}

void MapObject::GetName()
{
    mName = mStates["mName"];
}

void MapObject::Activate(bool direction)
{
    sol::protected_function f = mStates["Activate"];
    auto ret = f(direction);
    if (!ret.valid())
    {
        sol::error err = ret;
        std::string what = err.what();
        LOG_ERROR(what);
    }
}

bool MapObject::WallCollision(f32 dx, f32 dy) const
{
    // The game checks for both kinds of walls no matter the direction
    // ddcheat into a tunnel and the "inside out" wall will still force
    // a crouch.
    return
        CollisionLine::RayCast<2>(mMap.Lines(),
            glm::vec2(mXPos, mYPos + dy),
            glm::vec2(mXPos + (mFlipX ? -dx : dx), mYPos + dy),
            { 1u, 2u }, nullptr);
}

bool MapObject::CellingCollision(f32 dx, f32 dy) const
{
    return CollisionLine::RayCast<1>(mMap.Lines(),
        glm::vec2(mXPos + (mFlipX ? -dx : dx), mYPos - 2), // avoid collision if we are standing on a celling
        glm::vec2(mXPos + (mFlipX ? -dx : dx), mYPos + dy),
        { 3u }, nullptr);
}

std::tuple<bool, f32, f32, f32> MapObject::FloorCollision() const
{
    Physics::raycast_collision c;
    if (CollisionLine::RayCast<1>(mMap.Lines(),
        glm::vec2(mXPos, mYPos),
        glm::vec2(mXPos, mYPos + 260*3), // Check up to 3 screen down
        { 0u }, &c))
    {
        const f32 distance = glm::distance(mYPos, c.intersection.y);
        return std::make_tuple(true, c.intersection.x, c.intersection.y, distance);
    }
    return std::make_tuple(false, 0.0f, 0.0f, 0.0f);
}

void MapObject::LoadScript()
{
    // Load FSM script
    const std::string script = mLocator.LocateScript(mScriptName.c_str());
    try
    {
        mLuaState.script(script);
    }
    catch (const sol::error& /*ex*/)
    {
        //LOG_ERROR(ex.what());
        return;
    }

    // Set initial state
    try
    {
        sol::protected_function f = mLuaState["init"];
        auto ret = f(this);
        if (!ret.valid())
        {
            sol::error err = ret;
            std::string what = err.what();
            LOG_ERROR(what);
        }

    }
    catch (const sol::error& e)
    {
        LOG_ERROR(e.what());
    }
}

void MapObject::Update(const InputState& input)
{
    //TRACE_ENTRYEXIT;

    Debugging().mDebugObj = this;
    if (Debugging().mSingleStepObject && !Debugging().mDoSingleStepObject)
    {
        return;
    }

    //if (mAnim)
    {
        sol::protected_function f = mLuaState["update"];
        auto ret = f(this, input.Mapping().GetActions());
        if (!ret.valid())
        {
            sol::error err = ret;
            std::string what = err.what();
            LOG_ERROR(what);
        }
    }

    //::Sleep(300);

    static float prevX = 0.0f;
    static float prevY = 0.0f;
    if (prevX != mXPos || prevY != mYPos)
    {
        //LOG_INFO("Player X Delta " << mXPos - prevX << " Y Delta " << mYPos - prevY << " frame " << mAnim->FrameNumber());
    }
    prevX = mXPos;
    prevY = mYPos;

    Debugging().mInfo.mXPos = mXPos;
    Debugging().mInfo.mYPos = mYPos;
    Debugging().mInfo.mFrameToRender = FrameNumber();

    if (Debugging().mSingleStepObject && Debugging().mDoSingleStepObject)
    {
        // Step is done - no more updates till the user requests it
        Debugging().mDoSingleStepObject = false;
    }
}

bool MapObject::AnimationComplete() const
{
    if (!mAnim) { return false; }
    return mAnim->IsComplete();
}

void MapObject::SetAnimation(const std::string& animation)
{
    if (animation.empty())
    {
        mAnim = nullptr;
    }
    else
    {
        if (mAnims.find(animation) == std::end(mAnims))
        {
            auto anim = mLocator.LocateAnimation(animation.c_str());
            if (!anim)
            {
                LOG_ERROR("Animation " << animation << " not found");
                abort();
            }
            mAnims[animation] = std::move(anim);
        }
        mAnim = mAnims[animation].get();
        mAnim->Restart();
    }
}

void MapObject::SetAnimationFrame(s32 frame)
{
    if (mAnim)
    {
        mAnim->SetFrame(frame);
    }
}

void MapObject::SetAnimationAtFrame(const std::string& animation, u32 frame)
{
    SetAnimation(animation);
    mAnim->SetFrame(frame);
}

bool MapObject::AnimUpdate()
{
    return mAnim->Update();
}

s32 MapObject::FrameCounter() const
{
    return mAnim->FrameCounter();
}

s32 MapObject::NumberOfFrames() const
{
    return mAnim->NumberOfFrames();
}

bool MapObject::IsLastFrame() const
{
    return mAnim->IsLastFrame();
}

s32 MapObject::FrameNumber() const
{
    if (!mAnim) { return 0; }
    return mAnim->FrameNumber();
}

void MapObject::ReloadScript()
{
    LoadScript();
    SnapXToGrid();
}

void MapObject::Render(Renderer& rend, GuiContext& /*gui*/, int x, int y, float scale)
{
    if (mAnim)
    {
        mAnim->SetXPos(static_cast<s32>(mXPos)+x);
        mAnim->SetYPos(static_cast<s32>(mYPos)+y);
        mAnim->SetScale(scale);
        mAnim->Render(rend, mFlipX);
    }
}

bool MapObject::ContainsPoint(s32 x, s32 y) const
{
    if (!mAnim)
    {
        // For animationless objects use the object rect
        return PointInRect(x, y, mRect.x, mRect.y, mRect.w, mRect.h);
    }

    return mAnim->Collision(x, y);
}

void MapObject::SnapXToGrid()
{
    //25x20 grid hack
    const float oldX = mXPos;
    const s32 xpos = static_cast<s32>(mXPos);
    const s32 gridPos = (xpos - 12) % 25;
    if (gridPos >= 13)
    {
        mXPos = static_cast<float>(xpos - gridPos + 25);
    }
    else
    {
        mXPos = static_cast<float>(xpos - gridPos);
    }

    LOG_INFO("SnapX: " << oldX << " to " << mXPos);
}

// ============================================

Level::Level(IAudioController& /*audioController*/, ResourceLocator& locator, sol::state& luaState, Renderer& rend)
    : mLocator(locator), mLuaState(luaState)
{

    // Debugging - reload path and load next path
    static std::string currentPathName;
    static s32 nextPathIndex;
    Debugging().mFnNextPath = [&]() 
    {
        s32 idx = 0;
        for (const auto& pathMap : mLocator.mResMapper.mPathMaps)
        {
            if (idx == nextPathIndex)
            {
                std::unique_ptr<Oddlib::Path> path = mLocator.LocatePath(pathMap.first.c_str());
                if (path)
                {
                    mMap = std::make_unique<GridMap>(*path, mLocator, mLuaState, rend);
                    currentPathName = pathMap.first;
                    nextPathIndex = idx +1;
                    if (nextPathIndex > static_cast<s32>(mLocator.mResMapper.mPathMaps.size()))
                    {
                        nextPathIndex = 0;
                    }
                }
                else
                {
                    LOG_ERROR("LVL or file in LVL not found");
                }
                return;
            }
            idx++;
        }
    };

    Debugging().mFnReloadPath = [&]()
    {
        if (!currentPathName.empty())
        {
            std::unique_ptr<Oddlib::Path> path = mLocator.LocatePath(currentPathName.c_str());
            if (path)
            {
                mMap = std::make_unique<GridMap>(*path, mLocator, mLuaState, rend);
            }
            else
            {
                LOG_ERROR("LVL or file in LVL not found");
            }
        }
    };
}

void Level::EnterState()
{

}

void Level::Update(const InputState& input)
{
    if (mMap)
    {
        mMap->Update(input);
    }
}

void Level::Render(Renderer& rend, GuiContext& gui, int , int )
{
    if (Debugging().mShowBrowserUi)
    {
        RenderDebugPathSelection(rend, gui);
    }

    if (mMap)
    {
        mMap->Render(rend, gui);
    }
}

void Level::RenderDebugPathSelection(Renderer& rend, GuiContext& gui)
{
    gui_begin_window(&gui, "Paths");

    for (const auto& pathMap : mLocator.mResMapper.mPathMaps)
    {
        if (gui_button(&gui, pathMap.first.c_str()))
        {
            std::unique_ptr<Oddlib::Path> path = mLocator.LocatePath(pathMap.first.c_str());
            if (path)
            {
                mMap = std::make_unique<GridMap>(*path, mLocator, mLuaState, rend);
            }
            else
            {
                LOG_ERROR("LVL or file in LVL not found");
            }
            
        }
    }

    gui_end_window(&gui);
}

GridScreen::GridScreen(const std::string& lvlName, const Oddlib::Path::Camera& camera, Renderer& rend, ResourceLocator& locator)
    : mLvlName(lvlName)
    , mFileName(camera.mName)
    , mTexHandle(0)
    , mCamera(camera)
    , mLocator(locator)
    , mRend(rend)
{
   
}

GridScreen::~GridScreen()
{

}

int GridScreen::getTexHandle()
{
    if (!mTexHandle)
    {
        mCam = mLocator.LocateCamera(mFileName.c_str());
        if (mCam) // One path trys to load BRP08C10.CAM which exists in no data sets anywhere!
        {
            SDL_Surface* surf = mCam->GetSurface();
            mTexHandle = mRend.createTexture(GL_RGB, surf->w, surf->h, GL_RGB, GL_UNSIGNED_BYTE, surf->pixels, true);
        }
    }
    return mTexHandle;
}

bool GridScreen::hasTexture() const
{
    bool onlySpaces = true;
    for (size_t i = 0; i < mFileName.size(); ++i) 
    {
        if (mFileName[i] != ' ' && mFileName[i] != '\0')
        {
            onlySpaces = false;
            break;
        }
    }
    return !onlySpaces;
}

GridMap::GridMap(Oddlib::Path& path, ResourceLocator& locator, sol::state& luaState, Renderer& rend)
    : mPlayer(*this, luaState, locator, "abe.lua")
{
    mIsAo = path.IsAo();

    ConvertCollisionItems(path.CollisionItems());

    luaState.set_function("GetMapObject", &GridMap::GetMapObject, this);
    luaState.set_function("ActivateObjectsWithId", &GridMap::ActivateObjectsWithId, this);

    mScreens.resize(path.XSize());
    for (auto& col : mScreens)
    {
        col.resize(path.YSize());
    }

    for (u32 x = 0; x < path.XSize(); x++)
    {
        for (u32 y = 0; y < path.YSize(); y++)
        {
            mScreens[x][y] = std::make_unique<GridScreen>(mLvlName, path.CameraByPosition(x, y), rend, locator);
        }
    }

    mPlayer.Init();

    // TODO: Need to figure out what the right way to figure out where abe goes is
    // HACK: Place the player in the first screen that isn't blank
    for (auto x = 0u; x < mScreens.size(); x++)
    {
        for (auto y = 0u; y < mScreens[x].size(); y++)
        {
            GridScreen *screen = mScreens[x][y].get();
            if (screen->hasTexture())
            {
                const glm::vec2 camGapSize = (mIsAo) ? glm::vec2(1024, 480) : glm::vec2(375, 260);

                mPlayer.mXPos = (x * camGapSize.x) + 100.0f;
                mPlayer.mYPos = (y * camGapSize.y) + 100.0f;
            }
        }
    }
    mPlayer.SnapXToGrid();

    // Hack: Don't reload object_factory as the require statements only execute once
    // which results in the global factory table being cleared
    static bool factoryLoaded = false;
    if (!factoryLoaded)
    {
        const std::string script = locator.LocateScript("object_factory.lua");
        try
        {
            luaState.script(script);
        }
        catch (const sol::error& ex)
        {
            LOG_ERROR(ex.what()); // TODO: This is fatal
            return;
        }
        factoryLoaded = true;
    }

    // Load objects
    for (auto x = 0u; x < mScreens.size(); x++)
    {
        for (auto y = 0u; y < mScreens[x].size(); y++)
        {
            GridScreen* screen = mScreens[x][y].get();
            const Oddlib::Path::Camera& cam = screen->getCamera();
            for (size_t i = 0; i < cam.mObjects.size(); ++i)
            {
                const Oddlib::Path::MapObject& obj = cam.mObjects[i];
                Oddlib::MemoryStream ms(std::vector<u8>(obj.mData.data(), obj.mData.data() + obj.mData.size()));
                const ObjRect rect =
                {
                    obj.mRectTopLeft.mX,
                    obj.mRectTopLeft.mY,
                    obj.mRectBottomRight.mX - obj.mRectTopLeft.mX,
                    obj.mRectBottomRight.mY - obj.mRectTopLeft.mY
                };
                auto tmp = std::make_unique<MapObject>(*this, luaState, locator, rect);
                // Default "best guess" positioning
                tmp->mXPos = obj.mRectTopLeft.mX;
                tmp->mYPos = obj.mRectTopLeft.mY;

                sol::function f = luaState["object_factory"];
                Oddlib::IStream* s = &ms; // required else binding fails
                bool ret = f(*tmp, path.IsAo(), obj.mType, rect, *s);
                if (ret)
                {
                    tmp->GetName();
                    mObjs.push_back(std::move(tmp));
                }
            }
        }
    }
}

void GridMap::Update(const InputState& input)
{
    if (input.mKeys[SDL_SCANCODE_E].IsPressed())
    {
        if (mState == eStates::eEditor)
        {
            mState = eStates::eInGame;
            mPlayer.mXPos = mEditorCamOffset.x;
            mPlayer.mYPos = mEditorCamOffset.y;
        }
        else if (mState == eStates::eInGame)
        {
            mState = eStates::eEditor;
            mEditorCamOffset.x = mPlayer.mXPos;
            mEditorCamOffset.y = mPlayer.mYPos;
        }
    }

    f32 editorCamSpeed = 10.0f;

    if (input.mKeys[SDL_SCANCODE_LCTRL].IsDown())
    {
        if (input.mKeys[SDL_SCANCODE_W].IsPressed())
            mEditorCamZoom--;
        else if (input.mKeys[SDL_SCANCODE_S].IsPressed())
            mEditorCamZoom++;

        mEditorCamZoom = glm::clamp(mEditorCamZoom, 1, 15);
    }
    else
    {
        if (input.mKeys[SDL_SCANCODE_LSHIFT].IsDown())
            editorCamSpeed *= 4;

        if (input.mKeys[SDL_SCANCODE_W].IsDown())
            mEditorCamOffset.y -= editorCamSpeed;
        else if (input.mKeys[SDL_SCANCODE_S].IsDown())
            mEditorCamOffset.y += editorCamSpeed;

        if (input.mKeys[SDL_SCANCODE_A].IsDown())
            mEditorCamOffset.x -= editorCamSpeed;
        else if (input.mKeys[SDL_SCANCODE_D].IsDown())
            mEditorCamOffset.x += editorCamSpeed;
    }

    mPlayer.Update(input);

    for (std::unique_ptr<MapObject>& obj : mObjs)
    {
        obj->Update(input);
    }
}

MapObject* GridMap::GetMapObject(s32 x, s32 y, const char* type)
{
    for (std::unique_ptr<MapObject>& obj : mObjs)
    {
        if (obj->Name() == type)
        {
            if (obj->ContainsPoint(x, y))
            {
                return obj.get();
            }
        }
    }
    return nullptr;
}

void GridMap::ActivateObjectsWithId(MapObject* from, s32 id, bool direction)
{
    for (std::unique_ptr<MapObject>& obj : mObjs)
    {
        if (obj.get() != from && obj->Id() == id)
        {
            obj->Activate(direction);
        }
    }
}

/*static*/ CollisionLine::eLineTypes CollisionLine::ToType(u16 type, bool isAo)
{
    if (isAo)
    {
        // TODO: Implement me
        LOG_ERROR("No conversion of AO collision items yet");
        return eUnknown;
    }

    // TODO: Map of Ae and Ae collision lines to alive lines
    switch (type)
    {
    case eFloor: return eFloor;
    case eWallLeft: return eWallLeft;
    case eWallRight: return eWallRight;
    case eCeiling: return eCeiling;
    case eBackGroundFloor: return eBackGroundFloor;
    case eBackGroundWallLeft: return eBackGroundWallLeft;
    case eBackGroundWallRight: return eBackGroundWallRight;
    case eBackGroundCeiling: return eBackGroundCeiling;
    case eFlyingSligLine: return eFlyingSligLine;
    case eArt: return eArt;
    case eBulletWall: return eBulletWall;
    case eMineCarFloor: return eMineCarFloor;
    case eMineCarWall: return eMineCarWall;
    case eMineCarCeiling: return eMineCarCeiling;
    case eFlyingSligCeiling: return eFlyingSligCeiling;
    }
    LOG_ERROR("Unknown AE collision type: " << type);
    return eUnknown;
}

/*static*/ std::map<CollisionLine::eLineTypes, CollisionLine::LineData> CollisionLine::mData =
{
    { eFloor, {
        "Floor",
        { 255, 0, 0, 255 } }
    },
    { eWallLeft,
        { "Wall left",
        { 0, 0, 255, 255 }
    } },
    { eWallRight,
        { "Wall right",
        { 0, 100, 255, 255 }
    } },
    { eCeiling,
        { "Ceiling",
        { 255, 100, 0, 255 }
    } },
    { eBackGroundFloor,
        { "Bg floor",
        { 255, 100, 0, 255 }
    } },
    { eBackGroundWallLeft,
        { "Bg wall left",
        { 100, 100, 255, 255 }
    } },
    { eBackGroundWallRight,
        { "Bg wall right",
        { 0, 255, 255, 255 }
    } },
    { eBackGroundCeiling,
        { "Bg ceiling",
        { 255, 100, 0, 255 }
    } },
    { eFlyingSligLine,
        { "Flying slig line",
        { 255, 255, 0, 255 }
    } },
    { eArt,
        { "Art line",
        { 255, 255, 255, 255 }
    } },
    { eBulletWall,
        { "Bullet wall",
        { 255, 255, 0, 255 }
    } },
    { eMineCarFloor,
        { "Minecar floor",
        { 255, 255, 255, 255 }
    } },
    { eMineCarWall,
        { "Minecar wall",
        { 255, 0,   255, 255 }
    } },
    { eMineCarCeiling,
        { "Minecar ceiling",
        { 255, 0, 255, 255 }
    } },
    { eFlyingSligCeiling,
        { "Flying slig ceiling",
        { 255, 0, 255, 255 }
    } },
    { eUnknown,
        { "Unknown",
        { 255, 0, 255, 255 }
    } }
};

/*static*/ void CollisionLine::Render(Renderer& rend, const CollisionLines& lines)
{
    for (const std::unique_ptr<CollisionLine>& item : lines)
    {
        const glm::vec2 p1 = rend.WorldToScreen(item->mP1);
        const glm::vec2 p2 = rend.WorldToScreen(item->mP2);

        rend.lineCap(NVG_ROUND);
        rend.LineJoin(NVG_ROUND);
        rend.strokeColor(ColourF32{ 0, 0, 0, 1 });
        rend.strokeWidth(10.0f);
        rend.beginPath();
        rend.moveTo(p1.x, p1.y);
        rend.lineTo(p2.x, p2.y);
        rend.stroke();

        const auto it = mData.find(item->mType);
        assert(it != std::end(mData));

        rend.strokeColor(it->second.mColour.ToColourF32());
        rend.lineCap(NVG_BUTT);
        rend.LineJoin(NVG_BEVEL);
        rend.strokeWidth(4.0f);
        rend.beginPath();
        rend.moveTo(p1.x, p1.y);
        rend.lineTo(p2.x, p2.y);
        rend.stroke();

        rend.text(p1.x, p1.y, std::string(it->second.mName).c_str());
    }
}

void GridMap::RenderDebug(Renderer& rend)
{
    // Draw collisions
    if (Debugging().mCollisionLines)
    {
        CollisionLine::Render(rend, mCollisionItems);
    }

    // Draw grid
    if (Debugging().mGrid)
    {
        rend.strokeColor(ColourF32{ 1, 1, 1, 0.1f });
        rend.strokeWidth(2.f);
        int gridLineCountX = static_cast<int>((rend.mScreenSize.x / mEditorGridSizeX) / 2) + 2;
        for (int x = -gridLineCountX; x < gridLineCountX; x++)
        {
            rend.beginPath();
            glm::vec2 screenPos = rend.WorldToScreen(glm::vec2(rend.mCameraPosition.x + (x * mEditorGridSizeX) - (static_cast<int>(rend.mCameraPosition.x) % mEditorGridSizeX), 0));
            rend.moveTo(screenPos.x, 0);
            rend.lineTo(screenPos.x, static_cast<f32>(rend.mH));
            rend.stroke();
        }
        int gridLineCountY = static_cast<int>((rend.mScreenSize.y / mEditorGridSizeY) / 2) + 2;
        for (int y = -gridLineCountY; y < gridLineCountY; y++)
        {
            rend.beginPath();
            glm::vec2 screenPos = rend.WorldToScreen(glm::vec2(0, rend.mCameraPosition.y + (y * mEditorGridSizeY) - (static_cast<int>(rend.mCameraPosition.y) % mEditorGridSizeY)));
            rend.moveTo(0, screenPos.y);
            rend.lineTo(static_cast<f32>(rend.mW), screenPos.y);
            rend.stroke();
        }
    }

    // Draw objects
    if (Debugging().mObjectBoundingBoxes)
    {
        rend.strokeColor(ColourF32{ 1, 1, 1, 1 });
        rend.strokeWidth(1.f);
        for (auto x = 0u; x < mScreens.size(); x++)
        {
            for (auto y = 0u; y < mScreens[x].size(); y++)
            {
                GridScreen *screen = mScreens[x][y].get();
                if (!screen->hasTexture())
                    continue;
                const Oddlib::Path::Camera& cam = screen->getCamera();
                for (size_t i = 0; i < cam.mObjects.size(); ++i)
                {
                    const Oddlib::Path::MapObject& obj = cam.mObjects[i];

                    glm::vec2 topLeft = glm::vec2(obj.mRectTopLeft.mX, obj.mRectTopLeft.mY);
                    glm::vec2 bottomRight = glm::vec2(obj.mRectBottomRight.mX, obj.mRectBottomRight.mY);

                    glm::vec2 objPos = rend.WorldToScreen(glm::vec2(topLeft.x, topLeft.y));
                    glm::vec2 objSize = rend.WorldToScreen(glm::vec2(bottomRight.x, bottomRight.y)) - objPos;
                    rend.beginPath();
                    rend.rect(objPos.x, objPos.y, objSize.x, objSize.y);
                    rend.stroke();
                }
            }
        }
    }
}

void GridMap::RenderEditor(Renderer& rend, GuiContext& gui)
{
    //gui_begin_panel(&gui, "camArea");

    rend.mSmoothCameraPosition = true;

    rend.beginLayer(gui_layer(&gui) + 1);

    glm::vec2 camGapSize = (mIsAo) ? glm::vec2(1024, 480) : glm::vec2(375, 260);

    rend.mScreenSize = glm::vec2(rend.mW / 8, rend.mH / 8) * static_cast<f32>(mEditorCamZoom);

    rend.mCameraPosition = mEditorCamOffset;

    // Draw every cam
    for (auto x = 0u; x < mScreens.size(); x++)
    {
        for (auto y = 0u; y < mScreens[x].size(); y++)
        {
            GridScreen *screen = mScreens[x][y].get();
            if (!screen->hasTexture())
                continue;

            rend.drawQuad(screen->getTexHandle(), x * camGapSize.x, y * camGapSize.y, 368.0f, 240.0f);
        }
    }

    RenderDebug(rend);

    //const f32 zoomBase = 1.2f;
    //const f32 oldZoomMul = std::pow(zoomBase, 1.f*mZoomLevel);
    //bool zoomChanged = false;
    //if (gui.key_state[GUI_KEY_LCTRL] & GUI_KEYSTATE_DOWN_BIT)
    //{
    //    mZoomLevel += gui.mouse_scroll;
    //    zoomChanged = (gui.mouse_scroll != 0);
    //}
    //// Cap zooming so that things don't clump in the upper left corner
    //mZoomLevel = std::max(mZoomLevel, -12);

    //const f32 zoomMul = std::pow(zoomBase, 1.f*mZoomLevel);
    //// Use oldZoom because gui_set_frame_scroll below doesn't change scrolling in current frame. Could be changed though.
    //const int camSize[2] = { (int)(1440 * oldZoomMul), (int)(1080 * oldZoomMul) }; // TODO: Native reso should be constant somewhere
    //const int margin[2] = { (int)(3000 * oldZoomMul), (int)(3000 * oldZoomMul) };

    //int worldFrameSize[2] = { 375, 260 };
    //if (mIsAo)
    //{
    //    worldFrameSize[0] = 1024;
    //    worldFrameSize[1] = 480;
    //}
    //int worldCamSize[2] = { 368, 240 }; // Size of cam background in object coordinate system
    //f32 frameSize[2] = { 1.f * worldFrameSize[0] / worldCamSize[0] * camSize[0],
    //    1.f * worldFrameSize[1] / worldCamSize[1] * camSize[1] };

    //// Zoom around cursor
    //*if (zoomChanged)
    //{
    //    int scroll[2];
    //    gui_scroll(&gui, &scroll[0], &scroll[1]);
    //    f32 scaledCursorPos[2] = { 1.f*gui.cursor_pos[0], 1.f*gui.cursor_pos[1] };
    //    f32 oldClientPos[2] = { scroll[0] + scaledCursorPos[0], scroll[1] + scaledCursorPos[1] };
    //    f32 worldPos[2] = { oldClientPos[0] * (1.f / oldZoomMul), oldClientPos[1] * (1.f / oldZoomMul) };
    //    f32 newClientPos[2] = { worldPos[0] * zoomMul, worldPos[1] * zoomMul };
    //    f32 newScreenPos[2] = { newClientPos[0] - scaledCursorPos[0], newClientPos[1] - scaledCursorPos[1] };

    //    gui_set_scroll(&gui, (int)(newScreenPos[0] + 0.5f), (int)(newScreenPos[1] + 0.5f));
    //}*/

    //// Draw cam backgrounds
    //f32 offset[2] = { 0, 0 };
    //if (mIsAo)
    //{
    //    offset[0] = 257.f * camSize[0] / worldCamSize[0];
    //    offset[1] = 114.f * camSize[1] / worldCamSize[1];
    //}
    //for (auto x = 0u; x < mScreens.size(); x++)
    //{
    //    for (auto y = 0u; y < mScreens[x].size(); y++)
    //    {
    //        GridScreen *screen = mScreens[x][y].get();
    //        if (!screen->hasTexture())
    //            continue;

    //        int pos[2];
    //        gui_turtle_pos(&gui, &pos[0], &pos[1]);
    //        pos[0] += (int)(frameSize[0] * x + offset[0]) + margin[0];
    //        pos[1] += (int)(frameSize[1] * y + offset[1]) + margin[1];
    //        rend.drawQuad(screen->getTexHandle(), 1.0f*pos[0], 1.0f*pos[1], 1.0f*camSize[0], 1.0f*camSize[1]);
    //        gui_enlarge_bounding(&gui, pos[0] + camSize[0] + margin[0] * 2,
    //            pos[1] + camSize[1] + margin[1] * 2);
    //    }
    //}

    //// Draw collision lines
    //{
    //    rend.strokeColor(Color{ 0, 0, 1, 1 });
    //    rend.strokeWidth(2.f);
    //    int pos[2];
    //    gui_turtle_pos(&gui, &pos[0], &pos[1]);
    //    pos[0] += margin[0];
    //    pos[1] += margin[1];
    //    for (size_t i = 0; i < mCollisionItems.size(); ++i)
    //    {
    //        const Oddlib::Path::CollisionItem& item = mCollisionItems[i];
    //        int p1[2] = { (int)(1.f * item.mP1.mX * frameSize[0] / worldFrameSize[0]),
    //            (int)(1.f * item.mP1.mY * frameSize[1] / worldFrameSize[1]) };
    //        int p2[2] = { (int)(1.f * item.mP2.mX * frameSize[0] / worldFrameSize[0]),
    //            (int)(1.f * item.mP2.mY * frameSize[1] / worldFrameSize[1]) };
    //        rend.beginPath();
    //        rend.moveTo(pos[0] + p1[0] + 0.5f, pos[1] + p1[1] + 0.5f);
    //        rend.lineTo(pos[0] + p2[0] + 0.5f, pos[1] + p2[1] + 0.5f);
    //        rend.stroke();
    //    }
    //}

    //{ // Draw objects
    //    rend.strokeColor(Color{ 1, 1, 1, 1 });
    //    rend.strokeWidth(1.f);
    //    for (auto x = 0u; x < mScreens.size(); x++)
    //    {
    //        for (auto y = 0u; y < mScreens[x].size(); y++)
    //        {
    //            GridScreen *screen = mScreens[x][y].get();
    //            if (!screen->hasTexture())
    //                continue;

    //            int pos[2];
    //            gui_turtle_pos(&gui, &pos[0], &pos[1]);
    //            pos[0] += margin[0];
    //            pos[1] += margin[1];
    //            const Oddlib::Path::Camera& cam = screen->getCamera();
    //            for (size_t i = 0; i < cam.mObjects.size(); ++i)
    //            {
    //                const Oddlib::Path::MapObject& obj = cam.mObjects[i];
    //                int objPos[2] = { (int)(1.f * obj.mRectTopLeft.mX * frameSize[0] / worldFrameSize[0]),
    //                    (int)(1.f * obj.mRectTopLeft.mY * frameSize[1] / worldFrameSize[1]) };
    //                int objSize[2] = { (int)(1.f * (obj.mRectBottomRight.mX - obj.mRectTopLeft.mX) * frameSize[0] / worldFrameSize[0]),
    //                    (int)(1.f * (obj.mRectBottomRight.mY - obj.mRectTopLeft.mY) * frameSize[1] / worldFrameSize[1]) };

    //                rend.beginPath();
    //                rend.rect(pos[0] + 1.f*objPos[0] + 0.5f, pos[1] + 1.f*objPos[1] + 0.5f, 1.f*objSize[0], 1.f*objSize[1]);
    //                rend.stroke();
    //            }
    //        }
    //    }
    //}

    rend.endLayer();
    //gui_end_panel(&gui);
}

void GridMap::RenderGame(Renderer& rend, GuiContext& gui)
{
    if (Debugging().mShowDebugUi)
    {
        // Debug ui
        gui_begin_window(&gui, "Script debug");
        if (gui_button(&gui, "Reload abe script"))
        {
            mPlayer.ReloadScript();
        }
        gui_end_window(&gui);
    }

    rend.mSmoothCameraPosition = false;

    const glm::vec2 camGapSize = (mIsAo) ? glm::vec2(1024, 480) : glm::vec2(375, 260);

    rend.mScreenSize = glm::vec2(368, 240);
    const int camX = static_cast<int>(mPlayer.mXPos / camGapSize.x);
    const int camY = static_cast<int>(mPlayer.mYPos / camGapSize.y);

    rend.mCameraPosition = glm::vec2(camX * camGapSize.x, camY * camGapSize.y) + glm::vec2(368 / 2, 240 / 2);
    rend.updateCamera(); // TODO: this fixes headache inducing flicker on screen change, probably needs to go in Update()

    // Culling is disabled until proper camera position updating order is fixed
    // ^ not sure what this means, but rendering things at negative cam index seems to go wrong
    if (camX >= 0 && camY >= 0 && camX < static_cast<int>(mScreens.size()) && camY < static_cast<int>(mScreens[camX].size()))
    {
        GridScreen* screen = mScreens[camX][camY].get();
        if (screen->hasTexture())
        {
            rend.drawQuad(screen->getTexHandle(), camX * camGapSize.x, camY * camGapSize.y, 368.0f, 240.0f);
        }
    }
    
/*
    // For now draw every cam
    for (auto x = 0u; x < mScreens.size(); x++)
    {
        for (auto y = 0u; y < mScreens[x].size(); y++)
        {
            GridScreen* screen = mScreens[x][y].get();
            if (!screen->hasTexture())
            {
                continue;
            }

            rend.drawQuad(screen->getTexHandle(), x * camGapSize.x, y * camGapSize.y, 368.0f, 240.0f);
        }
    }
*/

    RenderDebug(rend);

    for (std::unique_ptr<MapObject>& obj : mObjs)
    {
        obj->Render(rend, gui, 0, 0, 1.0f);
    }

    mPlayer.Render(rend, gui,
        0,
        0,
        1.0f);

    // Test raycasting for shadows
    DebugRayCast(rend,
        glm::vec2(mPlayer.mXPos, mPlayer.mYPos),
        glm::vec2(mPlayer.mXPos, mPlayer.mYPos + 500),
        0,
        glm::vec2(0, -10)); // -10 so when we are *ON* a line you can see something

    DebugRayCast(rend,
        glm::vec2(mPlayer.mXPos, mPlayer.mYPos - 2),
        glm::vec2(mPlayer.mXPos, mPlayer.mYPos - 60),
        3,
        glm::vec2(0, 0));

    if (mPlayer.mFlipX)
    {
        DebugRayCast(rend,
            glm::vec2(mPlayer.mXPos, mPlayer.mYPos - 20),
            glm::vec2(mPlayer.mXPos - 25, mPlayer.mYPos - 20), 1);

        DebugRayCast(rend,
            glm::vec2(mPlayer.mXPos, mPlayer.mYPos - 50),
            glm::vec2(mPlayer.mXPos - 25, mPlayer.mYPos - 50), 1);
    }
    else
    {
        DebugRayCast(rend,
            glm::vec2(mPlayer.mXPos, mPlayer.mYPos - 20),
            glm::vec2(mPlayer.mXPos + 25, mPlayer.mYPos - 20), 2);

        DebugRayCast(rend,
            glm::vec2(mPlayer.mXPos, mPlayer.mYPos - 50),
            glm::vec2(mPlayer.mXPos + 25, mPlayer.mYPos - 50), 2);
    }
}

void GridMap::DebugRayCast(Renderer& rend, const glm::vec2& from, const glm::vec2& to, u32 collisionType, const glm::vec2& fromDrawOffset)
{
    if (Debugging().mRayCasts)
    {
        Physics::raycast_collision collision;
        if (CollisionLine::RayCast<1>(Lines(), from, to, { collisionType }, &collision))
        {
            const glm::vec2 fromDrawPos = rend.WorldToScreen(from + fromDrawOffset);
            const glm::vec2 hitPos = rend.WorldToScreen(collision.intersection);

            rend.strokeColor(ColourF32{ 1, 0, 1, 1 });
            rend.strokeWidth(2.f);
            rend.beginPath();
            rend.moveTo(fromDrawPos.x, fromDrawPos.y);
            rend.lineTo(hitPos.x, hitPos.y);
            rend.stroke();
        }
    }
}

static CollisionLine* GetCollisionIndexByIndex(CollisionLines& lines, s16 index)
{
    const s32 count = static_cast<s32>(lines.size());
    if (index > 0)
    {
        if (index < count)
        {
            return lines[index].get();
        }
        else
        {
            LOG_ERROR("Link index is out of bounds: " << index);
        }
    }
    return nullptr;
}

static void ConvertLink(CollisionLines& lines, const Oddlib::Path::Links& oldLink, CollisionLine::Link& newLink)
{
    newLink.mPrevious = GetCollisionIndexByIndex(lines, oldLink.mPrevious);
    newLink.mNext = GetCollisionIndexByIndex(lines, oldLink.mNext);
}

void GridMap::ConvertCollisionItems(const std::vector<Oddlib::Path::CollisionItem>& items)
{
    const s32 count = static_cast<s32>(items.size());
    mCollisionItems.resize(count);

    // First pass to create/convert from original/"raw" path format
    for (auto i = 0; i < count; i++)
    {
        mCollisionItems[i] = std::make_unique<CollisionLine>();
        mCollisionItems[i]->mP1.x = items[i].mP1.mX;
        mCollisionItems[i]->mP1.y = items[i].mP1.mY;

        mCollisionItems[i]->mP2.x = items[i].mP2.mX;
        mCollisionItems[i]->mP2.y = items[i].mP2.mY;

        mCollisionItems[i]->mType = CollisionLine::ToType(items[i].mType, mIsAo);
    }

    // Second pass to set up raw pointers to existing lines for connected segments of 
    // collision lines
    for (auto i = 0; i < count; i++)
    {
        // TODO: Check if optional link is ever used in conjunction with link
        ConvertLink(mCollisionItems, items[i].mLinks[0], mCollisionItems[i]->mLink);
        ConvertLink(mCollisionItems, items[i].mLinks[1], mCollisionItems[i]->mOptionalLink);
    }

    // Now we can re-order collision items without breaking prev/next links, thus we want to ensure
    // that anything that either has no links, or only a single prev/next links is placed first
    // so that we can render connected segments from the start or end.
    std::sort(std::begin(mCollisionItems), std::end(mCollisionItems), [](std::unique_ptr<CollisionLine>& a, std::unique_ptr<CollisionLine>& b)
    {
        return std::tie(a->mLink.mNext, a->mLink.mPrevious) < std::tie(b->mLink.mNext, b->mLink.mPrevious);
    });

    // Ensure that lines link together physically
    for (auto i = 0; i < count; i++)
    {
        // Some walls have next links, overlapping the walls will break them
        if (mCollisionItems[i]->mLink.mNext && mCollisionItems[i]->mType == CollisionLine::eFlyingSligLine)
        {
            mCollisionItems[i]->mP2 = mCollisionItems[i]->mLink.mNext->mP1;
        }
    }

    // TODO: Render connected segments as one with control points
}

void GridMap::Render(Renderer& rend, GuiContext& gui)
{
    if (mState == eStates::eEditor)
    {
        RenderEditor(rend, gui);
    }
    else if (mState == eStates::eInGame)
    {
        RenderGame(rend, gui);
    }
}
