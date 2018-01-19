#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <deque>
#include <future>
#include "core/audiobuffer.hpp"
#include "soundcache.hpp"

class GameData;
class IAudioController;
class IFileSystem;
class ResourceLocator;
class MusicTheme;
class MusicThemeEntry;

class ActiveMusicThemeEntry
{
public:
    ActiveMusicThemeEntry() = default;

    void SetMusicThemeEntry(const std::vector<MusicThemeEntry>* theme)
    {
        mActiveTheme = theme;
        mEntryIndex = 0;
    }

    bool ToNextEntry()
    {
        if (!mActiveTheme)
        {
            return false;
        }

        if (mEntryIndex+1 < mActiveTheme->size())
        {
            // TODO: Check loop count
            mEntryIndex++;
            return true;
        }
        return false;
    }

    const MusicThemeEntry* Entry() const
    {
        if (!mActiveTheme)
        {
            return nullptr;
        }
        return &(*mActiveTheme)[mEntryIndex];
    }
private:
    u32 mEntryIndex = 0;
    const std::vector<MusicThemeEntry>* mActiveTheme = nullptr;
};

class ISound;
class OSBaseFileSystem;
class JobSystem;

namespace Oddlib
{
    class MemoryStream;
}

using SoundId = u32;

class Sound : public IAudioPlayer
{
public:
    Sound(const Sound&) = delete;
    Sound& operator = (const Sound&) = delete;
    Sound(IAudioController& audioController, ResourceLocator& locator, OSBaseFileSystem& fs, JobSystem& jobSystem);
    ~Sound();

    void SetMusicTheme(const char* themeName, const char* eventOnLoad = nullptr);
    bool IsLoading() const;

    void StopAllMusic();
    void HandleMusicEvent(const char* eventName);
    SoundId PlaySoundEffect(const char* soundName);
    void StopSoundEffect(SoundId id);

    void Update();

private:
    void CacheActiveTheme(bool add);
    std::unique_ptr<ISound> PlaySound(const std::string& soundName, const std::string& explicitSoundBankName, bool useMusicRecord, bool useSfxRecord, bool useCache);
    void SoundBrowserUi();
    std::unique_ptr<ISound> PlayThemeEntry(const char* entryName);
    void EnsureAmbiance();
private: // IAudioPlayer
    virtual bool Play(f32* stream, u32 len) override;
private:
    IAudioController& mAudioController;
    ResourceLocator& mLocator;
    SoundCache mCache;

    const MusicTheme* mActiveTheme = nullptr;
    const MusicTheme* mThemeToLoad = nullptr;
    std::string mEventToSetAfterLoad;

    ActiveMusicThemeEntry mActiveThemeEntry;

    std::mutex mSoundPlayersMutex;

    // Thread safe
    std::unique_ptr<ISound> mAmbiance;

    // Thread safe
    std::unique_ptr<ISound> mMusicTrack;

    // Thread safe
    std::map<SoundId, std::unique_ptr<ISound>> mSoundPlayers;

    static std::atomic<SoundId> mSoundId;

    enum class eSoundStates
    {
        eLoadSoundEffects,
        eLoadingSoundEffects,
        eUnloadingActiveSoundTheme,
        eLoadActiveSoundTheme,
        eLoadingActiveSoundTheme,
        eCancel,
        eCancelling,
        eIdle
    };
    eSoundStates mState = eSoundStates::eIdle;

    void SetState(Sound::eSoundStates state);

    // Shared to avoid locking
    std::shared_ptr<class SequencePlayer> mSoundBankBeingBrowsed;
};
