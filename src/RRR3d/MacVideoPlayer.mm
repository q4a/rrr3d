#include "MacVideoPlayer.h"

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace rrr3d::video
{

struct MacVideoPlayer::Impl
{
    __weak NSWindow* window = nil;
    __strong AVPlayer* player = nil;
    __strong AVPlayerLayer* layer = nil;
    __strong NSView* hostView = nil;
    __strong id completionObserver = nil;
    std::atomic_bool completed{false};
    bool active = false;
};

MacVideoPlayer::MacVideoPlayer(void* nativeWindow)
    : impl_(std::make_unique<Impl>())
{
    impl_->window = (__bridge NSWindow*)nativeWindow;
}

MacVideoPlayer::~MacVideoPlayer()
{
    stop();
}

bool MacVideoPlayer::play(const std::filesystem::path& path, float volume,
                          std::string& error)
{
    stop();
    if (impl_->window == nil || impl_->window.contentView == nil)
    {
        error = "Cocoa content view is unavailable";
        return false;
    }
    const auto pathString = path.string();
    NSString* filePath =
        [NSString stringWithUTF8String:pathString.c_str()];
    if (filePath == nil ||
        ![[NSFileManager defaultManager] fileExistsAtPath:filePath])
    {
        error = "movie cache is missing: " + pathString;
        return false;
    }

    NSURL* url = [NSURL fileURLWithPath:filePath];
    AVPlayerItem* item = [AVPlayerItem playerItemWithURL:url];
    impl_->player = [AVPlayer playerWithPlayerItem:item];
    impl_->player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
    impl_->player.automaticallyWaitsToMinimizeStalling = YES;
    impl_->player.volume = std::clamp(volume, 0.0F, 1.0F);
    impl_->layer =
        [AVPlayerLayer playerLayerWithPlayer:impl_->player];
    impl_->layer.videoGravity = AVLayerVideoGravityResizeAspect;
    impl_->layer.backgroundColor = NSColor.blackColor.CGColor;

    NSView* contentView = impl_->window.contentView;
    // Do not attach AVPlayerLayer to SDL's Metal view/layer tree. A separate
    // topmost Cocoa view owns movie composition and its drawable lifetime.
    impl_->hostView = [[NSView alloc] initWithFrame:contentView.bounds];
    impl_->hostView.wantsLayer = YES;
    impl_->hostView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    impl_->hostView.layer = impl_->layer;
    [contentView addSubview:impl_->hostView positioned:NSWindowAbove relativeTo:nil];
    [CATransaction commit];

    impl_->completed.store(false);
    Impl* state = impl_.get();
    impl_->completionObserver =
        [[NSNotificationCenter defaultCenter]
            addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
                        object:item
                         queue:NSOperationQueue.mainQueue
                    usingBlock:^(NSNotification*) {
                        state->completed.store(true);
                    }];
    impl_->active = true;
    [impl_->player play];
    return true;
}

void MacVideoPlayer::stop()
{
    if (!impl_)
        return;
    if (impl_->player != nil)
        [impl_->player pause];
    if (impl_->completionObserver != nil)
    {
        [[NSNotificationCenter defaultCenter]
            removeObserver:impl_->completionObserver];
        impl_->completionObserver = nil;
    }
    if (impl_->layer != nil)
        [impl_->layer removeFromSuperlayer];
    [impl_->hostView removeFromSuperview];
    impl_->hostView = nil;
    impl_->layer = nil;
    impl_->player = nil;
    impl_->completed.store(false);
    impl_->active = false;
}

void MacVideoPlayer::resize()
{
    if (impl_->layer != nil && impl_->window.contentView != nil)
    {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        impl_->hostView.frame = impl_->window.contentView.bounds;
        [CATransaction commit];
    }
}

void MacVideoPlayer::seek(double seconds)
{
    if (impl_->player == nil || !std::isfinite(seconds))
        return;
    const auto time =
        CMTimeMakeWithSeconds(std::max(seconds, 0.0), 600);
    [impl_->player seekToTime:time
             toleranceBefore:kCMTimeZero
              toleranceAfter:kCMTimeZero];
}

PlaybackState MacVideoPlayer::update(std::string& error) const
{
    if (!impl_->active)
        return PlaybackState::Idle;
    if (impl_->completed.load())
        return PlaybackState::Completed;
    AVPlayerItem* item = impl_->player.currentItem;
    if (item != nil && item.status == AVPlayerItemStatusFailed)
    {
        NSString* description = item.error.localizedDescription;
        error = description != nil
                    ? std::string(description.UTF8String)
                    : std::string("AVPlayerItem failed");
        return PlaybackState::Failed;
    }
    return PlaybackState::Playing;
}

void MacVideoPlayer::waitForNextUpdate()
{
    @autoreleasepool
    {
        [CATransaction flush];
        // SDL's nonblocking event poll is not an AppKit application run loop.
        // Let timed media/main-queue callbacks run instead of sleeping the
        // main thread, and return promptly for keyboard/mouse skip events.
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0 / 120.0, true);
    }
}

double MacVideoPlayer::positionSeconds() const
{
    if (impl_->player == nil)
        return 0.0;
    const double value = CMTimeGetSeconds(impl_->player.currentTime);
    return std::isfinite(value) ? value : 0.0;
}

bool MacVideoPlayer::readyForDisplay() const
{
    return impl_->layer != nil && impl_->layer.readyForDisplay;
}

bool MacVideoPlayer::hasAudioTrack() const
{
    if (impl_->player == nil || impl_->player.currentItem == nil)
        return false;
    AVAsset* asset = impl_->player.currentItem.asset;
    return asset != nil &&
           [asset tracksWithMediaType:AVMediaTypeAudio].count > 0U;
}

double MacVideoPlayer::durationSeconds() const
{
    if (impl_->player == nil || impl_->player.currentItem == nil)
        return 0.0;
    const double duration =
        CMTimeGetSeconds(impl_->player.currentItem.duration);
    return std::isfinite(duration) ? duration : 0.0;
}

} // namespace rrr3d::video
