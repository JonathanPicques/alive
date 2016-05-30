#include "resourcemapper.hpp"
#include "zipfilesystem.hpp"

bool DataPaths::SetActiveDataPaths(IFileSystem& fs, const DataSetMap& paths)
{
    mActiveDataPaths.clear();

    // Add paths in order, including mod zips
    for (const PriorityDataSet& pds : paths)
    {
        if (!pds.mDataSetPath.empty())
        {
            auto dataSetFs = IFileSystem::Factory(fs, pds.mDataSetPath);
            if (dataSetFs)
            {
                mActiveDataPaths.emplace_back(FileSystemInfo(pds.mDataSetName, pds.mSourceGameDefinition->IsMod(), std::move(dataSetFs)));
            }
            else
            {
                // Couldn't get an FS for the data path, fail
                return false;
            }
        }
    }
    return true;
}

/*static*/ std::unique_ptr<IFileSystem> IFileSystem::Factory(IFileSystem& fs, const std::string& path)
{
    TRACE_ENTRYEXIT;

    if (path.empty())
    {
        return nullptr;
    }

    const bool isFile = fs.FileExists(path.c_str());
    std::unique_ptr<IFileSystem> ret;
    if (isFile)
    {
        if (string_util::ends_with(path, ".bin", true))
        {
            LOG_INFO("Creating ISO FS for " << path);
            ret = std::make_unique<CdIsoFileSystem>(path.c_str());
        }
        else if (string_util::ends_with(path, ".zip", true))
        {
            LOG_INFO("Creating ZIP FS for " << path);
            ret = std::make_unique<ZipFileSystem>(path.c_str(), fs);
        }
        else
        {
            LOG_ERROR("Unknown archive type for: " << path);
            return nullptr;
        }
    }
    else
    {
        LOG_INFO("Creating dir view FS for " << path);
        ret = std::make_unique<DirectoryLimitedFileSystem>(fs, path);
    }

    if (ret)
    {
        if (!ret->Init())
        {
            LOG_ERROR("FS init failed");
            return nullptr;
        }
    }
    return ret;
}
