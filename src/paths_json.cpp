#include "paths_json.hpp"
#include "proxy_rapidjson.hpp"

const PathsJson::PathLocation* PathsJson::PathMapping::Find(const std::string& dataSetName) const
{
    for (const PathLocation& location : mLocations)
    {
        if (location.mDataSetName == dataSetName)
        {
            return &location;
        }
    }
    return nullptr;
}

const PathsJson::PathMapping* PathsJson::FindPath(const std::string& resourceName)
{
    const auto it = mPathMaps.find(resourceName);
    if (it != std::end(mPathMaps))
    {
        return &it->second;
    }
    return nullptr;
}

void PathsJson::FromJson(rapidjson::Document& doc)
{
    const auto& docRootArray = doc.GetArray();
    for (auto& it : docRootArray)
    {
        if (it.HasMember("paths"))
        {
            const rapidjson::Value::Array& pathsArray = it["paths"].GetArray();
            for (const rapidjson::Value& obj : pathsArray)
            {
                FromJson(obj);
            }
        }
    }
}

void PathsJson::FromJson(const rapidjson::Value& obj)
{
    PathMapping mapping;
    mapping.mId = obj["id"].GetInt();
    mapping.mCollisionOffset = obj["collision_offset"].GetInt();
    mapping.mIndexTableOffset = obj["object_indextable_offset"].GetInt();
    mapping.mObjectOffset = obj["object_offset"].GetInt();
    mapping.mNumberOfScreensX = obj["number_of_screens_x"].GetInt();
    mapping.mNumberOfScreensY = obj["number_of_screens_y"].GetInt();

    if (obj.HasMember("music_theme"))
    {
        mapping.mMusicTheme = obj["music_theme"].GetString();
    }

    const auto& locations = obj["locations"].GetArray();
    for (auto& locationRecord : locations)
    {
        const auto& dataSet = locationRecord["dataset"].GetString();
        const auto& dataSetFileName = locationRecord["file_name"].GetString();
        mapping.mLocations.push_back(PathLocation{ dataSet, dataSetFileName });
    }

    const auto& name = obj["resource_name"].GetString();
    mPathMaps[name] = mapping;
}

std::string PathsJson::NextPathName(s32& idx)
{
    // Loop back to the start
    if (static_cast<size_t>(idx) > mPathMaps.size())
    {
        idx = 0;
    }

    // Find in the map by index
    int i = 0;
    for (auto& it : mPathMaps)
    {
        if (i == idx)
        {
            idx++;
            return it.first;
        }
        i++;

    }

    // Impossible unless the map is empty
    return "";
}

std::map<std::string, PathsJson::PathMapping> PathsJson::Map() const
{
    return mPathMaps;
}
