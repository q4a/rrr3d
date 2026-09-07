#pragma once

#include "physics/OriginalVehiclePhysics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace r3d::effects
{

// Source FxEmitter::OnProgress(sotDist), QueryCreateGroup and
// QueryCreateParticles(mnaWaitingFree), specialized for the shipped trail
// record: fixed distance/life/density=1, world coordinates and zero flow.
// Group life starts at maxLife even when a catch-up particle has nonzero
// particle time. FxTrailManager uses group life for its material frame.
class FxTrailEmitter
{
public:
    struct Group
    {
        physics::Vec3 position;
        float life = 0.0F;
        float maxLife = 0.0F;
        float particleTime = 0.0F;
        std::uint64_t index = 0U;

        float GetFrame() const noexcept
        {
            return maxLife > 0.0F ? 1.0F - life / maxLife : 0.0F;
        }
    };

    FxTrailEmitter(float distance, float life, std::size_t maximum)
        : distance_(std::max(distance, 0.001F)), life_(life),
          maximum_(maximum)
    {
    }

    void OnProgress(float deltaTime, physics::Vec3 position, bool fading)
    {
        if (!std::isfinite(deltaTime) || !std::isfinite(position.x) ||
            !std::isfinite(position.y) || !std::isfinite(position.z))
            return;
        deltaTime = std::max(deltaTime, 0.0F);
        currentTime_ += deltaTime;
        position_ = position;
        if (groups_.empty())
        {
            lastQueryPosition_ = position_;
            lastQueryTime_ = currentTime_;
        }
        std::erase_if(groups_, [deltaTime](Group& group) {
            if (group.maxLife > 0.0F)
            {
                group.life = std::max(group.life - deltaTime, 0.0F);
                if (group.life <= 0.0F)
                    return true;
            }
            group.particleTime += deltaTime;
            return false;
        });
        if (fading)
            return;
        if (groups_.empty())
        {
            nextDistance_ = distance_;
            QueryCreateGroup(0.0F, {});
            return;
        }
        const auto relative = Subtract(groups_.back().position, position_);
        const float length = std::sqrt(relative.x * relative.x +
            relative.y * relative.y + relative.z * relative.z);
        float remaining = std::min(length - nextDistance_, distance_ * 20.0F);
        const auto offset = Subtract(lastQueryPosition_, position_);
        const float elapsed = currentTime_ - lastQueryTime_;
        while (remaining > 0.0F)
        {
            const float fraction = remaining / length;
            QueryCreateGroup(elapsed * fraction,
                {offset.x * fraction, offset.y * fraction,
                 offset.z * fraction});
            nextDistance_ = distance_;
            remaining -= nextDistance_;
        }
    }

    const std::vector<Group>& GetGroups() const noexcept { return groups_; }
    std::size_t GetCntParticle() const noexcept { return groups_.size(); }
    physics::Vec3 GetWorldPos() const noexcept { return position_; }

    float GetRemainingLife() const noexcept
    {
        float result = 0.0F;
        for (const auto& group : groups_)
        {
            if (group.maxLife <= 0.0F)
                return -1.0F;
            result = std::max(result, group.life);
        }
        return result;
    }

private:
    static physics::Vec3 Subtract(physics::Vec3 a, physics::Vec3 b) noexcept
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    void QueryCreateGroup(float deltaTime, physics::Vec3 offset)
    {
        // Source updates these even when mnaWaitingFree rejects creation.
        lastQueryTime_ = currentTime_;
        lastQueryPosition_ = position_;
        if (maximum_ > 0U && groups_.size() >= maximum_)
            return;
        groups_.push_back(
            {{position_.x + offset.x, position_.y + offset.y,
              position_.z + offset.z}, life_, life_, deltaTime, nextIndex_++});
    }

    std::vector<Group> groups_;
    physics::Vec3 position_;
    physics::Vec3 lastQueryPosition_;
    float currentTime_ = 0.0F;
    float lastQueryTime_ = 0.0F;
    float nextDistance_ = 0.0F;
    float distance_;
    float life_;
    std::size_t maximum_;
    std::uint64_t nextIndex_ = 0U;
};

} // namespace r3d::effects
