#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace rrr3d::video
{

enum class PlaybackState
{
    Idle,
    Playing,
    Completed,
    Failed,
};

class MacVideoPlayer
{
public:
    explicit MacVideoPlayer(void* nativeWindow);
    ~MacVideoPlayer();

    MacVideoPlayer(const MacVideoPlayer&) = delete;
    MacVideoPlayer& operator=(const MacVideoPlayer&) = delete;

    bool play(const std::filesystem::path& path, float volume,
              std::string& error);
    void stop();
    void resize();
    void seek(double seconds);
    PlaybackState update(std::string& error) const;
    bool readyForDisplay() const;
    bool hasAudioTrack() const;
    double durationSeconds() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rrr3d::video
