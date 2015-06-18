#include "oddlib/masher.hpp"
#include "oddlib/exceptions.hpp"
#include "oddlib/lvlarchive.hpp"
#include "logger.hpp"

namespace Oddlib
{
    void Masher::Read()
    {
        mStream.ReadUInt32(mFileHeader.mDdvTag);
        if (mFileHeader.mDdvTag != MakeType('D', 'D', 'V', 0))
        {
            LOG_ERROR("Invalid DDV magic tag %X", mFileHeader.mDdvTag);
            throw Exception("Invalid DDV tag");
        }

        mStream.ReadUInt32(mFileHeader.mDdvVersion);
        if (mFileHeader.mDdvVersion != 1)
        {
            // This is the only version seen in all of the known data
            LOG_ERROR("Expected DDV version to be 2 but got %d", mFileHeader.mDdvVersion);
            throw Exception("Wrong DDV version");
        }

        mStream.ReadUInt32(mFileHeader.mContains);
        mStream.ReadUInt32(mFileHeader.mFrameRate);
        mStream.ReadUInt32(mFileHeader.mNumberOfFrames);

        mbHasVideo = (mFileHeader.mContains & 0x1) == 0x1;
        mbHasAudio = (mFileHeader.mContains & 0x2) == 0x2;

        if (mbHasVideo)
        {
            mStream.ReadUInt32(mVideoHeader.mUnknown);
            mStream.ReadUInt32(mVideoHeader.mWidth);
            mStream.ReadUInt32(mVideoHeader.mHeight);
            mStream.ReadUInt32(mVideoHeader.mMaxVideoFrameSize);
            mStream.ReadUInt32(mVideoHeader.mMaxAudioFrameSize);
            mStream.ReadUInt32(mVideoHeader.mKeyFrameRate);

            mNumMacroblocksX = (mVideoHeader.mWidth / 16);
            if (mVideoHeader.mWidth % 16 != 0)
            {
                mNumMacroblocksX++;
            }

            mNumMacroblocksY = (mVideoHeader.mHeight / 16);
            if (mVideoHeader.mHeight % 16 != 0)
            {
                mNumMacroblocksY++;
            }
        }

        if (mbHasAudio)
        {
            mStream.ReadUInt32(mAudioHeader.mAudioFormat);
            mStream.ReadUInt32(mAudioHeader.mSampleRate);
            mStream.ReadUInt32(mAudioHeader.mMaxAudioFrameSize);
            mStream.ReadUInt32(mAudioHeader.mSingleAudioFrameSize);
            mStream.ReadUInt32(mAudioHeader.mNumberOfFramesInterleave);
            

            for (uint32_t i = 0; i < mAudioHeader.mNumberOfFramesInterleave; i++)
            {
                uint32_t tmp = 0;
                mStream.ReadUInt32(tmp);
                mAudioFrameSizes.emplace_back(tmp);
            }
        }

        for (uint32_t i = 0; i < mFileHeader.mNumberOfFrames; i++)
        {
            uint32_t tmp = 0;
            mStream.ReadUInt32(tmp);
            mVideoFrameSizes.emplace_back(tmp);
        }
        
    }

    bool Masher::Update()
    {
        if (mCurrentFrame < mFileHeader.mNumberOfFrames)
        {
            mCurrentFrame++;
            return false;
        }
        return true;
    }
}
