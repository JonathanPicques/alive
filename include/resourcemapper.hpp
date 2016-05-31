#pragma once

#include "jsonxx/jsonxx.h"
#include <unordered_map>
#include <regex>
#include <map>
#include <set>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "string_util.hpp"
#include "oddlib/cdromfilesystem.hpp"
#include "logger.hpp"

#include "oddlib/lvlarchive.hpp"
#include "oddlib/anim.hpp"
#include "renderer.hpp"

namespace JsonDeserializer
{
    template<typename T>
    void ReadStringArray(const jsonxx::Object& jsonObject, const std::string& jsonArrayName, T& resultingStrings)
    {
        if (jsonObject.has<jsonxx::Array>(jsonArrayName))
        {
            const jsonxx::Array& stringArray = jsonObject.get<jsonxx::Array>(jsonArrayName);
            for (const auto& f : stringArray.values())
            {
                resultingStrings.insert(resultingStrings.end(), f->get<jsonxx::String>());
            }
        }
    }
}

inline size_t StringHash(const char* s)
{
    // FNV hasher
    size_t result = 2166136261U;
    while (*s)
    {
        result = (16777619 * result) ^ static_cast<unsigned char>(*s);
        s++;
    }
    return result;
}

inline size_t StringHash(const std::string& s)
{
    return StringHash(s.c_str());
}

inline std::vector<Uint8> StringToVector(const std::string& str)
{
    return std::vector<Uint8>(str.begin(), str.end());
}

class IFileSystem
{
public:
    IFileSystem() = default;
    IFileSystem(const IFileSystem&) = delete;
    IFileSystem& operator = (const IFileSystem&) = delete;

    virtual ~IFileSystem() = default;
    
    virtual bool Init() { return true; }
    virtual std::unique_ptr<Oddlib::IStream> Open(const std::string& fileName) = 0;

    virtual std::vector<std::string> EnumerateFiles(const std::string& directory, const char* filter) = 0;
    virtual bool FileExists(const std::string& fileName) = 0;
    virtual std::string FsPath() const = 0;

    enum EMatchType
    {
        IgnoreCase,
        MatchCase
    };

    static std::unique_ptr<IFileSystem> Factory(IFileSystem& fs, const std::string& path);
protected:
    struct DirectoryAndFileName
    {
        DirectoryAndFileName(std::string path)
        {
            NormalizePath(path);

            auto lastDirPos = path.find_last_of('/');
            if (lastDirPos != std::string::npos)
            {
                mDir = path.substr(0, lastDirPos);
                mFile = path.substr(lastDirPos + 1);
            }
            else
            {
                mFile = path;
            }
        }

        std::string mDir;
        std::string mFile;
    };

    static void NormalizePath(std::string& path)
    {
        string_util::replace_all(path, "\\", "/");
        string_util::replace_all(path, "//", "/");
    }

    static bool WildCardMatcher(const std::string& text, std::string wildcardPattern, EMatchType caseSensitive)
    {
        // Escape all regex special chars
        EscapeRegex(wildcardPattern);

        // Convert chars '*?' back to their regex equivalents
        string_util::replace_all(wildcardPattern, "\\?", ".");
        string_util::replace_all(wildcardPattern, "\\*", ".*");

        std::regex pattern(wildcardPattern,
            caseSensitive == MatchCase ?
            std::regex_constants::ECMAScript :
            std::regex_constants::ECMAScript | std::regex_constants::icase);

        return std::regex_match(text, pattern);
    }

    static void EscapeRegex(std::string& regex)
    {
        string_util::replace_all(regex, "\\", "\\\\");
        string_util::replace_all(regex, "^", "\\^");
        string_util::replace_all(regex, ".", "\\.");
        string_util::replace_all(regex, "$", "\\$");
        string_util::replace_all(regex, "|", "\\|");
        string_util::replace_all(regex, "(", "\\(");
        string_util::replace_all(regex, ")", "\\)");
        string_util::replace_all(regex, "[", "\\[");
        string_util::replace_all(regex, "]", "\\]");
        string_util::replace_all(regex, "*", "\\*");
        string_util::replace_all(regex, "+", "\\+");
        string_util::replace_all(regex, "?", "\\?");
        string_util::replace_all(regex, "/", "\\/");
    }
};


class OSBaseFileSystem : public IFileSystem
{
private:
    static bool IsDots(const std::string& name)
    {
        return name == "." || name == "..";
    }
public:

    virtual bool Init() override
    {
        return true;
    }

    virtual std::unique_ptr<Oddlib::IStream> Open(const std::string& fileName) override
    {
        return std::make_unique<Oddlib::Stream>(ExpandPath(fileName));
    }

#ifdef _WIN32
    struct FindCloseDeleter
    {
        typedef HANDLE pointer;
        void operator()(HANDLE hFind)
        {
            if (hFind != INVALID_HANDLE_VALUE)
            {
                ::FindClose(hFind);
            }
        }
    };
    typedef std::unique_ptr<HANDLE, FindCloseDeleter> FindCloseHandle;

    virtual std::vector<std::string> EnumerateFiles(const std::string& directory, const char* filter) override
    {
        std::vector<std::string> ret;
        WIN32_FIND_DATA findData = {};
        const std::string dirPath = ExpandPath(directory) + "/" + filter;
        FindCloseHandle ptr(::FindFirstFile(dirPath.c_str(), &findData));
        if (ptr.get() != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!IsDots(findData.cFileName) && !(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    ret.emplace_back(findData.cFileName);
                }
            } while (::FindNextFile(ptr.get(), &findData));
            LOG_INFO(ret.size() << " items enumerated from " << dirPath);
        }
        else
        {
            LOG_ERROR("Failed to enumerate directory " << dirPath);
        }
        return ret;
    }
#else
    struct closedirDeleter
    {
        typedef DIR* pointer;
        void operator()(DIR* dir)
        {
            closedir(dir);
        }
    };
    typedef std::unique_ptr<DIR, closedirDeleter> closedirHandle;

    virtual std::vector<std::string> EnumerateFiles(const std::string& directory, const char* filter) override
    {
        std::vector<std::string> ret;
        const std::string dirPath = ExpandPath(directory) + "/";
        closedirHandle dir(opendir(dirPath.c_str()));
        if (dir)
        {
            dirent* ent = nullptr;
            do
            {
                const std::string strFilter(filter);
                ent = readdir(dir.get());
                if (ent)
                {
                    const std::string dirName = ent->d_name;
                    if (!IsDots(dirName) && WildCardMatcher(dirName, strFilter, IgnoreCase))
                    {
                        ret.emplace_back(dirName);
                        LOG_INFO("Name is " << dirName);
                    }
                }
            } while (ent);
            LOG_INFO(ret.size() << " items enumerated from " << dirPath);
        }
        else
        {
            LOG_ERROR("Failed to enumerate directory " << dirPath);
        }
        return ret;
    }
#endif

#ifdef _WIN32
    bool FileExists(const std::string& fileName) override
    {
        const auto name = ExpandPath(fileName);
        const DWORD dwAttrib = GetFileAttributes(name.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }
#else
    bool FileExists(const std::string& fileName) override
    {
        struct stat buffer = {};
        return stat(ExpandPath(fileName).c_str(), &buffer) == 0;
    }
#endif

    virtual std::string ExpandPath(const std::string& path) = 0;
};

class GameFileSystem : public OSBaseFileSystem
{
public:
    virtual bool Init() override final
    {
        auto basePath = InitBasePath();
        if (basePath.empty())
        {
            return false;
        }
        mNamedPaths["{GameDir}"] = basePath;

        // TODO: Resolve {UserDir}
        mNamedPaths["{UserDir}"] = ".";

        return true;
    }

    virtual std::string FsPath() const override
    {
        return mNamedPaths.find("{GameDir}")->second;
    }

    std::string InitBasePath()
    {
        char* pBasePath = SDL_GetBasePath();
        std::string basePath;
        if (pBasePath)
        {
            basePath = pBasePath;
            SDL_free(pBasePath);

            // If it looks like we're running from the IDE/dev build then attempt to fix up the path to the correct location to save
            // manually setting the correct working directory.
            const bool bIsDebugPath = string_util::contains(basePath, "/alive/bin/") || string_util::contains(basePath, "\\alive\\bin\\");
            if (bIsDebugPath)
            {
                if (string_util::contains(basePath, "/alive/bin/"))
                {
                    LOG_WARNING("We appear to be running from the IDE (Linux) - fixing up basePath to be ../");
                    basePath += "../";
                }
                else
                {
                    LOG_WARNING("We appear to be running from the IDE (Win32) - fixing up basePath to be ../");
                    basePath += "..\\..\\";
                }
            }
        }
        else
        {
            LOG_ERROR("SDL_GetBasePath failed");
            return "";
        }
        LOG_INFO("basePath is " << basePath);
        NormalizePath(basePath);
        return basePath;
    }

    virtual std::string ExpandPath(const std::string& path) override final
    {
        std::string ret = path;
        for (const auto& namedPath : mNamedPaths)
        {
            string_util::replace_all(ret, namedPath.first, namedPath.second);
        }
        NormalizePath(ret);
        return ret;
    }

private:
    std::map<std::string, std::string> mNamedPaths;
};

class DirectoryLimitedFileSystem : public IFileSystem
{
public:
    DirectoryLimitedFileSystem(IFileSystem& fs, const std::string& directory)
        : mFs(fs)
    {
        mBasePath = directory;
        NormalizePath(mBasePath);
    }

    virtual std::string FsPath() const override
    {
        return mBasePath;
    }

    virtual bool Init() override final
    {
        return true;
    }

    virtual std::unique_ptr<Oddlib::IStream> Open(const std::string& fileName) override final
    {
        return mFs.Open(LimitPath(fileName));
    }

    virtual std::vector<std::string> EnumerateFiles(const std::string& directory, const char* filter) override final
    {
        return mFs.EnumerateFiles(LimitPath(directory), filter);
    }

    virtual bool FileExists(const std::string& fileName) override final
    {
        return mFs.FileExists(LimitPath(fileName));
    }

private:
    std::string LimitPath(const std::string& path)
    {
        return mBasePath + "/" + path;
    }

    IFileSystem& mFs;
    std::string mBasePath;
};

class CdIsoFileSystem : public IFileSystem
{
public:
    explicit CdIsoFileSystem(const char* fileName)
        : mRawCdImage(fileName), mCdImagePath(fileName)
    {

    }

    virtual ~CdIsoFileSystem() = default;

    virtual std::string FsPath() const override
    {
        return mCdImagePath;
    }

    virtual bool Init() override
    {
        // TODO
        return true;
    }

    virtual std::unique_ptr<Oddlib::IStream> Open(const std::string& fileName) override
    {
        return mRawCdImage.ReadFile(fileName, false);
    }

    virtual std::vector<std::string> EnumerateFiles(const std::string& /*directory*/, const char* /*filter*/) override
    {
        // TODO
        abort();
        //return std::vector<std::string> { };
    }

    virtual bool FileExists(const std::string& fileName) override
    {
        return mRawCdImage.FileExists(fileName) != -1;
    }

private:
    RawCdImage mRawCdImage;
    std::string mCdImagePath;
};

class DataPathIdentities
{
public:
    DataPathIdentities() = default;

    DataPathIdentities(IFileSystem& fs, const char* dataSetsIdsFileName)
    {
        auto stream = fs.Open(dataSetsIdsFileName);
        if (stream)
        {
            Parse(stream->LoadAllToString());
        }
    }

    bool IsMetaPath(const std::string& id) const
    {
        return mMetaPaths.find(id) != std::end(mMetaPaths);
    }

    std::string Identify(IFileSystem& fs, const std::string& path) const
    {
        for (const auto& dataPathId : mDataPathIds)
        {
            if (MatchPathWithDataPathId(fs, dataPathId, path))
            {
                return dataPathId.first;
            }
        }
        return "";
    }

private:
    struct DataPathFiles
    {
        std::vector<std::string> mContainAnyOf;
        std::vector<std::string> mContainAllOf;
        std::vector<std::string> mMustNotContain;
    };

    std::map<std::string, DataPathFiles> mDataPathIds;

    // A meta path is one that exists just to include other paths
    // e.g AePsx includes AePsxCd1 and AxePsxCd1, but AePsx won't actually have a path as such
    std::set<std::string> mMetaPaths;

    bool DoMatchPathWithDataPathId(IFileSystem& fs, const std::pair<std::string, DataPathFiles>& dataPathId) const
    {
        // Check all of the "must exist files" do exist
        for (const auto& f : dataPathId.second.mContainAllOf)
        {
            if (!fs.FileExists(f.c_str()))
            {
                // Skip this dataPathId, the path doesn't match all of the "must contain"
                return false;
            }
        }

        // Check that all of the "must not exist" files don't exist
        for (const auto& f : dataPathId.second.mMustNotContain)
        {
            if (fs.FileExists(f.c_str()))
            {
                // Found a file that shouldn't exist, skip to the next dataPathId
                return false;
            }
        }

        // Check we can find at least one of the "any of" set of files
        bool foundAnyOf = false;
        for (const auto& f : dataPathId.second.mContainAnyOf)
        {
            if (fs.FileExists(f.c_str()))
            {
                foundAnyOf = true;
                break;
            }
        }

        // If one of the "any of" files exists or we have one or more "must exist" files
        // then this dataPathId matches path
        if (foundAnyOf || dataPathId.second.mContainAllOf.empty() == false)
        {
            return true;
        }

        return false;
    }

    bool MatchPathWithDataPathId(IFileSystem& fs, const std::pair<std::string, DataPathFiles>& dataPathId, const std::string& path) const
    {
        auto dataSetFs = IFileSystem::Factory(fs, path);
        if (dataSetFs)
        {
            return DoMatchPathWithDataPathId(*dataSetFs, dataPathId);
        }
        return false;
    }

    void Parse(const std::string& json)
    {
        jsonxx::Object root;
        root.parse(json);
        if (root.has<jsonxx::Object>("data_set_ids"))
        {
            jsonxx::Object dataSetIds = root.get<jsonxx::Object>("data_set_ids");
            for (const auto& v : dataSetIds.kv_map())
            {
                const jsonxx::Object& dataSetId = dataSetIds.get<jsonxx::Object>(v.first);

                DataPathFiles dpFiles;
                JsonDeserializer::ReadStringArray(dataSetId, "contains_any", dpFiles.mContainAnyOf);
                JsonDeserializer::ReadStringArray(dataSetId, "contains_all", dpFiles.mContainAllOf);
                JsonDeserializer::ReadStringArray(dataSetId, "not_contains", dpFiles.mMustNotContain);

                if (dpFiles.mContainAnyOf.empty() && dpFiles.mContainAllOf.empty() && dpFiles.mMustNotContain.empty())
                {
                    mMetaPaths.insert(v.first);
                }
                else
                {
                    mDataPathIds[v.first] = dpFiles;
                }
            }
        }
    }
};

struct PriorityDataSet
{
public:
    PriorityDataSet(std::string dataSetName, const class GameDefinition* sourceGameDefinition)
        : mDataSetName(dataSetName), mSourceGameDefinition(sourceGameDefinition)
    {

    }

    bool operator == (const PriorityDataSet& other) const
    {
        return (mDataSetName == other.mDataSetName && mSourceGameDefinition == other.mSourceGameDefinition);
    }

    std::string mDataSetName;
    std::string mDataSetPath;
    const GameDefinition* mSourceGameDefinition;
};

using DataSetMap = std::vector<PriorityDataSet>;

class DataPaths
{
public:
    DataPaths(const DataPaths&) = delete;
    DataPaths& operator = (const DataPaths&) = delete;

    DataPaths(DataPaths&& other)
    {
        *this = std::move(other);
    }

    DataPaths& operator = (DataPaths&& other)
    {
        mIds = std::move(other.mIds);
        mPaths = std::move(other.mPaths);
        return *this;
    }

    DataPaths(IFileSystem& fs, const char* dataSetsIdsFileName, const char* dataPathFileName)
        : mIds(fs, dataSetsIdsFileName)
    {
        if (fs.FileExists(dataPathFileName))
        {
            auto stream = fs.Open(dataPathFileName);
            std::vector<std::string> paths = Parse(stream->LoadAllToString());
            for (const auto& path : paths)
            {
                std::string id = mIds.Identify(fs, path);
                if (id.empty())
                {
                    LOG_ERROR("Path " << path << " could not be identified");
                    continue;
                }
              
                auto it = mPaths.find(id);
                if (it == std::end(mPaths))
                {
                    LOG_INFO("Path " << path << " identified as " << id);
                    mPaths[id] = path;
                }
                else
                {
                    LOG_INFO("Path " << path 
                        << " identified as " << id 
                        << " but ignoring because we already have the following path "
                        << it->second 
                        << " for " << id);
                }
            }
        }
    }

    const std::string& PathFor(const std::string& id)
    {
        auto it = mPaths.find(id);
        if (it == std::end(mPaths))
        {
            return mNotFoundResult;
        }
        else
        {
            return it->second;
        }
    }

    std::vector<std::string> MissingDataSetPaths(const std::vector<std::string>& requiredSets)
    {
        std::vector<std::string> ret;
        for (const auto& dataset : requiredSets)
        {
            if (mIds.IsMetaPath(dataset))
            {
                continue;
            }

            if (!PathFor(dataset).empty())
            {
                continue;
            }

            ret.emplace_back(dataset);
        }
        return ret;
    }

    bool SetActiveDataPaths(IFileSystem& fs, const DataSetMap& paths);

    struct FileSystemInfo
    {
        FileSystemInfo(const std::string& name, bool isMod, std::unique_ptr<IFileSystem> fs)
            : mDataSetName(name), mIsMod(isMod), mFileSystem(std::move(fs))
        {

        }

        FileSystemInfo(FileSystemInfo&& rhs)
        {
            *this = std::move(rhs);
        }

        FileSystemInfo& operator = (FileSystemInfo&& rhs)
        {
            mDataSetName = std::move(rhs.mDataSetName);
            mIsMod = rhs.mIsMod;
            mFileSystem = std::move(rhs.mFileSystem);
            return *this;
        }

        std::string mDataSetName;
        bool mIsMod;
        std::unique_ptr<IFileSystem> mFileSystem;
    };
    const std::vector<FileSystemInfo>& ActiveDataPaths() const { return mActiveDataPaths; }
private:

    std::vector<FileSystemInfo> mActiveDataPaths;

    std::map<std::string, std::string> mPaths;

    std::vector<std::string> Parse(const std::string& json)
    {
        std::vector<std::string> paths;
        jsonxx::Object root;
        root.parse(json);
        if (root.has<jsonxx::Array>("paths"))
        {
            JsonDeserializer::ReadStringArray(root, "paths", paths);
        }
        return paths;
    }

    // To match to what a game def wants (AePcCd1, AoDemoPsx etc)
    // we use SLUS codes for PSX or if it contains ABEWIN.EXE etc then its AoPc.
    DataPathIdentities mIds;
    const /*static*/ std::string mNotFoundResult;
};


struct BuiltInAndModGameDefs
{
    std::vector< const PriorityDataSet* > gameDefs;
    std::vector< const PriorityDataSet* > modDefs;
};

class GameDefinition
{
private:
    static bool Exists(const std::string& dataSetName, const DataSetMap& dataSets)
    {
        for (const PriorityDataSet& dataSet : dataSets)
        {
            if (dataSet.mDataSetName == dataSetName)
            {
                return true;
            }
        }
        return false;
    }

    static const GameDefinition* Find(const std::string& dataSetName, const std::vector<const GameDefinition*>& gds)
    {
        for (const GameDefinition* gd : gds)
        {
            if (gd->DataSetName() == dataSetName)
            {
                return gd;
            }
        }
        return nullptr;
    }

    static void GetDependencies(int& priority, DataSetMap& requiredDataSets, std::set<std::string>& missingDataSets, const GameDefinition* gd, const std::vector<const GameDefinition*>& gds)
    {
        requiredDataSets.emplace_back(gd->DataSetName(), gd);
        for (const std::string& dataSetName : gd->RequiredDataSets())
        {
            // Skip if already processed to avoid inf recursion on cycles
            if (!Exists(dataSetName, requiredDataSets))
            {
                // Find the game def that contains the dataset that the current game def requires
                const GameDefinition* foundGameDef = Find(dataSetName, gds);
                if (foundGameDef)
                {
                    // Recursively check the this game defs dependencies too
                    GetDependencies(priority, requiredDataSets, missingDataSets, foundGameDef, gds);
                }
                else
                {
                    missingDataSets.insert(dataSetName);
                }
            }
        }
    }

public:
    static BuiltInAndModGameDefs SplitInToBuiltInAndMods(const DataSetMap& requiredDataSets)
    {
        BuiltInAndModGameDefs sorted;
        for (const auto& dataSetPair : requiredDataSets)
        {
            if (dataSetPair.mSourceGameDefinition->IsMod())
            {
                sorted.modDefs.push_back(&dataSetPair);
            }
            else
            {
                sorted.gameDefs.push_back(&dataSetPair);
            }
        }
        return sorted;
    }

    // DFS graph walk
    // This means if we have:
    // mod -> [ AePsx, AePc]
    // Where AePsx depends on AePsxCd1 and AePsxCd1 then the search order of resources would be:
    // mod, AePsx, AePsxCd1, AePsxCd2, AePc.
    static void GetDependencies(DataSetMap& requiredDataSets, std::set<std::string>& missingDataSets, const GameDefinition* gd, const std::vector<const GameDefinition*>& gds)
    {
        int priority = 0;
        GetDependencies(priority, requiredDataSets, missingDataSets, gd, gds);
    }

    static std::vector<const GameDefinition*> GetVisibleGameDefinitions(const std::vector<GameDefinition>& gameDefinitions)
    {
        std::vector<const GameDefinition*> ret;
        ret.reserve(gameDefinitions.size());
        for (const auto& gd : gameDefinitions)
        {
            if (!gd.Hidden())
            {
                ret.emplace_back(&gd);
            }
        }
        return ret;
    }

    GameDefinition(const GameDefinition&) = default;
    GameDefinition& operator = (const GameDefinition&) = default;

    GameDefinition(IFileSystem& fileSystem, const char* gameDefinitionFile, bool isMod)
        : mIsMod(isMod)
    {
        auto stream = fileSystem.Open(gameDefinitionFile);
        assert(stream != nullptr);
        const auto jsonData = stream->LoadAllToString();
        Parse(jsonData);
        mContainingArchive = fileSystem.FsPath();
    }

    GameDefinition(std::string name, std::string dataSetName, std::vector<std::string> requiredDataSets, bool isMod)
        : mName(name), mDataSetName(dataSetName), mRequiredDataSets(requiredDataSets), mIsMod(isMod)
    {

    }

    GameDefinition() = default;
    
    const std::string& Name() const { return mName; }
    const std::string& Description() const { return mDescription; }
    const std::string& Author() const { return mAuthor; }
    const std::string& InitialLevel() const { return mInitialLevel; }
    const std::string& DataSetName() const { return mDataSetName; }
    const std::vector<std::string> RequiredDataSets() const { return mRequiredDataSets; }
    bool Hidden() const { return mHidden; }
    bool IsMod() const { return mIsMod; }
    const std::string ContainingArchive() const { return mContainingArchive; }
private:

    void Parse(const std::string& json)
    {
        jsonxx::Object root;
        root.parse(json);
        
        mName = root.get<jsonxx::String>("Name");
        mDescription = root.get<jsonxx::String>("Description");
        mAuthor = root.get<jsonxx::String>("Author");
        mInitialLevel = root.get<jsonxx::String>("InitialLevel");
        mDataSetName = root.get<jsonxx::String>("DatasetName");
        if (root.has<jsonxx::Boolean>("Hidden"))
        {
            mHidden = root.get<jsonxx::Boolean>("Hidden");
        }

        if (root.has<jsonxx::Array>("RequiredDatasets"))
        {
            const auto requiredSets = root.get<jsonxx::Array>("RequiredDatasets");
            mRequiredDataSets.reserve(requiredSets.size());
            for (size_t i = 0; i < requiredSets.size(); i++)
            {
                mRequiredDataSets.emplace_back(requiredSets.get<jsonxx::String>(static_cast<Uint32>(i)));
            }
        }
    }

    std::string mName;
    std::string mDescription;
    std::string mAuthor;
    std::string mInitialLevel;
    std::string mDataSetName; // Name of this data set
    bool mHidden = false;
    std::vector<std::string> mRequiredDataSets;
    bool mIsMod = false;
    std::string mContainingArchive;
};

class ResourceMapper
{
public:
    ResourceMapper() = default;

    ResourceMapper(ResourceMapper&& rhs)
    {
        *this = std::move(rhs);
    }

    ResourceMapper& operator = (ResourceMapper&& rhs)
    {
        if (this != &rhs)
        {
            mAnimMaps = std::move(rhs.mAnimMaps);
            mFileLocations = std::move(rhs.mFileLocations);
        }
        return *this;
    }

    ResourceMapper(IFileSystem& fileSystem, const char* resourceMapFile)
    {
        auto stream = fileSystem.Open(resourceMapFile);
        assert(stream != nullptr);
        const auto jsonData = stream->LoadAllToString();
        Parse(jsonData);
    }

    struct AnimMapping
    {
        std::string mFile;
        Uint32 mId;
        Uint32 mBlendingMode;
    };

    std::pair<AnimMapping*, std::map<std::string, std::vector<std::pair<bool, std::string>>>*> Find(const char* resourceName)
    {
        auto am = mAnimMaps.find(resourceName);
        if (am != std::end(mAnimMaps))
        {
            auto fl = mFileLocations.find(am->second.mFile);
            if (fl != std::end(mFileLocations))
            {
                return std::make_pair(&am->second, &fl->second);
            }
            else
            {
                return std::make_pair(&am->second, nullptr);
            }
        }
        return std::make_pair(nullptr, nullptr);
    }

    void AddAnimMapping(const std::string& resourceName, const AnimMapping& mapping)
    {
        mAnimMaps[resourceName] = mapping;
    }
private:

    std::map<std::string, AnimMapping> mAnimMaps;

    void Parse(const std::string& json)
    {
        jsonxx::Array root;
        if (!root.parse(json))
        {
            throw std::runtime_error("Can't parse resource map json");
        }

        for (size_t rootObjIndex = 0; rootObjIndex < root.size(); rootObjIndex++)
        {
            jsonxx::Object obj = root.get<jsonxx::Object>(static_cast<unsigned int>(rootObjIndex));
            if (obj.has<jsonxx::Array>("anims"))
            {
                ParseAnimResourceJson(obj);
            }
            else if (obj.has<jsonxx::Array>("lvls"))
            {
                ParseFileLocations(obj);
            }
        }
    }

    std::map<std::string, std::map<std::string, std::vector<std::pair<bool,std::string>>>> mFileLocations;

    void ParseFileLocations(const jsonxx::Object& obj)
    {
        // This is slightly tricky as we reverse the mapping of the data that is in the json file
        // the json file maps a data set, if its PSX or not, its lvls and lvl contents.
        // But we want to map a file name to what LVLs it lives in, and a LVL to what data sets it lives in
        // and if that data set is PSX or not.
        const std::string& dataSetName = obj.get<jsonxx::String>("data_set_name");
        const bool isPsx = obj.get<jsonxx::Boolean>("is_psx");
        
        const jsonxx::Array& lvls = obj.get<jsonxx::Array>("lvls");
        for (size_t i = 0; i < lvls.size(); i++)
        {
            const jsonxx::Object& lvlRecord = lvls.get<jsonxx::Object>(static_cast<Uint32>(i));
            const std::string& lvlName = lvlRecord.get<jsonxx::String>("name");

            std::set<std::string> lvlFiles;
            JsonDeserializer::ReadStringArray(lvlRecord, "files", lvlFiles);
            for (const std::string& fileName : lvlFiles)
            {
                mFileLocations[fileName][dataSetName].push_back(std::make_pair(isPsx, lvlName));
            }
        }
    }

    void ParseAnimResourceJson(const jsonxx::Object& obj)
    {
        const jsonxx::Array& anims = obj.get<jsonxx::Array>("anims");

        const auto& file = obj.get<jsonxx::String>("file");
        const auto id = static_cast<Uint32>(obj.get<jsonxx::Number>("id"));

        for (size_t i = 0; i < anims.size(); i++)
        {
            AnimMapping mapping;
            mapping.mFile = file;
            mapping.mId = static_cast<Uint32>(id);

            const jsonxx::Object& animRecord = anims.get<jsonxx::Object>(static_cast<Uint32>(i));

            const auto& name = animRecord.get<jsonxx::String>("name");
            const auto blendMode = animRecord.get<jsonxx::Number>("blend_mode");
            mapping.mBlendingMode = static_cast<Uint32>(blendMode);

            AddAnimMapping(name, mapping);
        }
    }

};

class ResourceBase
{
public:
    virtual ~ResourceBase() = default;
    virtual void Reload() = 0;
};

class Animation : public ResourceBase
{
public:
    Animation() = delete;

    Animation(std::unique_ptr<Oddlib::IStream> stream, bool isPsx)
    {
        // TODO
        if (stream)
        {
            Oddlib::AnimSerializer as(*stream, isPsx);
            mAnim = std::make_unique<Oddlib::AnimationSet>(as);
        }
    }
    
    void Animate(Renderer& rend)
    {
        const Oddlib::Animation* anim = mAnim->AnimationAt(animNum);
        const Oddlib::Animation::Frame& frame = anim->GetFrame(frameNum);
        counter++;
        if (counter > 25)
        {
            counter = 0;
            frameNum++;
            if (frameNum >= anim->NumFrames())
            {
                frameNum = 0;
                animNum++;
                if (animNum >= mAnim->NumberOfAnimations())
                {
                    animNum = 0;
                }
            }
        }

        const int textureId = rend.createTexture(GL_RGBA, frame.mFrame->w, frame.mFrame->h, GL_RGBA, GL_UNSIGNED_BYTE, frame.mFrame->pixels, true);

        int scale = 3;
        float xpos = 300.0f + (frame.mOffX*scale);
        float ypos = 300.0f + (frame.mOffY*scale);
        // LOG_INFO("Pos " << xpos << "," << ypos);
        BlendMode blend = BlendMode::B100F100(); // TODO: Detect correct blending
        Color color = Color::white();
        rend.drawQuad(textureId, xpos, ypos, static_cast<float>(frame.mFrame->w*scale), static_cast<float>(frame.mFrame->h*scale), color, blend);

        rend.destroyTexture(textureId);
    }

    virtual void Reload() override
    {
        // TODO
    }

private:
    Uint32 counter = 0;
    Uint32 frameNum = 0;
    Uint32 animNum = 0;
    std::unique_ptr<Oddlib::AnimationSet> mAnim;
};

// TODO: Handle resources that have been loaded via explicit dataset name
class ResourceCache
{
public:
    void Add(size_t resourceHash, std::shared_ptr<ResourceBase> resource)
    {
        mCache[resourceHash] = resource;
    }

    void Remove(size_t resourceHash)
    {
        auto it = mCache.find(resourceHash);
        if (it != std::end(mCache))
        {
            auto sPtr = it->second.lock();
            if (!sPtr)
            {
                mCache.erase(it);
            }
        }
    }

    template<class T>
    std::shared_ptr<T> Find(size_t resourceHash)
    {
        auto it = mCache.find(resourceHash);
        if (it != std::end(mCache))
        {
            auto sPtr = it->second.lock();
            if (!sPtr)
            {
                mCache.erase(it);
                return nullptr;
            }
            return std::static_pointer_cast<T>(sPtr);
        }
        return nullptr;
    }
private:
    std::unordered_map<size_t, std::weak_ptr<ResourceBase>> mCache;
};

template<class T>
class Resource
{
public:
    Resource() = delete;

    Resource(ResourceCache& cache, std::shared_ptr<T> ptr, size_t resourceNameHash)
        : mResourceNameHash(resourceNameHash), mCache(cache)
    {
        mPtr = ptr;
    }

    Resource(const Resource& rhs)
        : mCache(rhs.mCache)
    {
        *this = rhs;
    }

    Resource& operator = (const Resource& rhs)
    {
        if (this != &rhs)
        {
            mResourceNameHash = rhs.mResourceNameHash;
            mPtr = rhs.mPtr;
        }
        return *this;
    }

    Resource(size_t resourceNameHash, ResourceCache& cache, std::unique_ptr<Oddlib::IStream> stream, bool isPsx)
        : mResourceNameHash(resourceNameHash), mCache(cache)
    {
        mPtr = std::make_shared<T>(std::move(stream), isPsx);
        mCache.Add(mResourceNameHash, mPtr);
    }

    ~Resource()
    {
        mCache.Remove(mResourceNameHash);
    }

    void Reload()
    {
        mPtr->Reload();
    }

    T* Ptr()
    {
        return mPtr.get();
    }

private:
    std::shared_ptr<T> mPtr;
    size_t mResourceNameHash;
    ResourceCache& mCache;
};


class ResourceLocator
{
public:
    ResourceLocator(const ResourceLocator&) = delete;
    ResourceLocator& operator =(const ResourceLocator&) = delete;


    ResourceLocator(ResourceMapper&& resourceMapper, DataPaths&& dataPaths)
        : mResMapper(std::move(resourceMapper)), mDataPaths(std::move(dataPaths))
    {

    }

    // TOOD: Provide limited interface to this?
    DataPaths& GetDataPaths()
    {
        return mDataPaths;
    }

    Resource<Animation> Locate(const char* resourceName);

    // This method should be used for debugging only - i.e so we can compare what resource X looks like
    // in dataset A and B.
    Resource<Animation> Locate(const char* resourceName, const std::string& dataSetName);

private:
    ResourceMapper mResMapper;
    DataPaths mDataPaths;
    ResourceCache mResourceCache;
};
