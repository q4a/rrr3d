#include "OriginalGameObject.h"

#include "OriginalLogic.h"
#include "OriginalMap.h"
#include "OriginalMapObj.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace r3d::game::originalrace::source
{

namespace
{

using SyncVector = GameObjectFrameSync::Vector;
using SyncQuaternion = GameObjectFrameSync::Quaternion;

constexpr float syncPi = 3.14159265358979323846F;
constexpr std::array<const char*, 15U> behaviorTypeNames{
    "btTouchDeath", "btResurrectObj", "btFxSystemWaitingEnd",
    "btFxSystemSrcSpeed", "btLowLifePoints", "btDamageEffect",
    "btDeathEffect", "btLifeEffect", "btSlowEffect",
    "btPxWheelSlipEffect", "btShotEffect", "btImmortalEffect",
    "btSoundMotor", "btGusenizaAnim", "btPodushkaAnim"};

SyncVector subtractSync(SyncVector left, SyncVector right) noexcept
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

float lengthSync(SyncVector value) noexcept
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

SyncVector normalizedSync(SyncVector value) noexcept
{
    const float valueLength = lengthSync(value);
    if (valueLength <= 0.000001F)
        return {};
    return {
        value.x / valueLength, value.y / valueLength,
        value.z / valueLength};
}

SyncQuaternion normalizedSync(SyncQuaternion value) noexcept
{
    const float valueLength = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (valueLength <= 0.000001F)
        return {};
    return {
        value.x / valueLength, value.y / valueLength,
        value.z / valueLength, value.w / valueLength};
}

SyncQuaternion multiplySync(
    SyncQuaternion left, SyncQuaternion right) noexcept
{
    return normalizedSync({
        left.w * right.x + left.x * right.w +
            left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z +
            left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y -
            left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x -
            left.y * right.y - left.z * right.z});
}

SyncQuaternion inverseSync(SyncQuaternion value) noexcept
{
    value = normalizedSync(value);
    return {-value.x, -value.y, -value.z, value.w};
}

SyncVector lerpSync(
    SyncVector from, SyncVector to, float alpha) noexcept
{
    alpha = std::clamp(alpha, 0.0F, 1.0F);
    return {
        from.x + (to.x - from.x) * alpha,
        from.y + (to.y - from.y) * alpha,
        from.z + (to.z - from.z) * alpha};
}

SyncQuaternion slerpSync(
    SyncQuaternion from, SyncQuaternion to, float alpha) noexcept
{
    from = normalizedSync(from);
    to = normalizedSync(to);
    alpha = std::clamp(alpha, 0.0F, 1.0F);
    float cosine = from.x * to.x + from.y * to.y +
                   from.z * to.z + from.w * to.w;
    if (cosine < 0.0F)
    {
        to = {-to.x, -to.y, -to.z, -to.w};
        cosine = -cosine;
    }
    if (cosine > 0.9995F)
    {
        return normalizedSync({
            from.x + (to.x - from.x) * alpha,
            from.y + (to.y - from.y) * alpha,
            from.z + (to.z - from.z) * alpha,
            from.w + (to.w - from.w) * alpha});
    }
    const float angle = std::acos(std::clamp(cosine, -1.0F, 1.0F));
    const float sine = std::sin(angle);
    if (std::abs(sine) <= 0.000001F)
        return from;
    const float fromWeight = std::sin((1.0F - alpha) * angle) / sine;
    const float toWeight = std::sin(alpha * angle) / sine;
    return normalizedSync({
        from.x * fromWeight + to.x * toWeight,
        from.y * fromWeight + to.y * toWeight,
        from.z * fromWeight + to.z * toWeight,
        from.w * fromWeight + to.w * toWeight});
}

GameObject::Quaternion normalizedProxy(
    GameObject::Quaternion value) noexcept
{
    const float length = std::sqrt(
        value[0] * value[0] + value[1] * value[1] +
        value[2] * value[2] + value[3] * value[3]);
    if (length <= 0.000001F)
        return {0.0F, 0.0F, 0.0F, 1.0F};
    return {value[0] / length, value[1] / length,
            value[2] / length, value[3] / length};
}

GameObject::Quaternion multiplyProxy(
    GameObject::Quaternion left,
    GameObject::Quaternion right) noexcept
{
    left = normalizedProxy(left);
    right = normalizedProxy(right);
    return normalizedProxy({
        left[3] * right[0] + left[0] * right[3] +
            left[1] * right[2] - left[2] * right[1],
        left[3] * right[1] - left[0] * right[2] +
            left[1] * right[3] + left[2] * right[0],
        left[3] * right[2] + left[0] * right[1] -
            left[1] * right[0] + left[2] * right[3],
        left[3] * right[3] - left[0] * right[0] -
            left[1] * right[1] - left[2] * right[2]});
}

GameObject::Quaternion inverseProxy(
    GameObject::Quaternion value) noexcept
{
    value = normalizedProxy(value);
    return {-value[0], -value[1], -value[2], value[3]};
}

GameObject::Vector3 rotateProxy(
    GameObject::Quaternion rotation,
    GameObject::Vector3 value) noexcept
{
    rotation = normalizedProxy(rotation);
    const GameObject::Vector3 twiceCross{
        2.0F * (rotation[1] * value[2] - rotation[2] * value[1]),
        2.0F * (rotation[2] * value[0] - rotation[0] * value[2]),
        2.0F * (rotation[0] * value[1] - rotation[1] * value[0])};
    return {
        value[0] + rotation[3] * twiceCross[0] +
            rotation[1] * twiceCross[2] - rotation[2] * twiceCross[1],
        value[1] + rotation[3] * twiceCross[1] +
            rotation[2] * twiceCross[0] - rotation[0] * twiceCross[2],
        value[2] + rotation[3] * twiceCross[2] +
            rotation[0] * twiceCross[1] - rotation[1] * twiceCross[0]};
}

SyncQuaternion rotationSync(
    SyncQuaternion current, SyncQuaternion next) noexcept
{
    return multiplySync(next, inverseSync(current));
}

float angleSync(SyncQuaternion value) noexcept
{
    value = normalizedSync(value);
    return 2.0F * std::acos(std::clamp(value.w, -1.0F, 1.0F));
}

SyncVector axisSync(SyncQuaternion value) noexcept
{
    value = normalizedSync(value);
    const float divisor = std::sqrt(std::max(
        1.0F - value.w * value.w, 0.0F));
    if (divisor <= 0.000001F)
        return {1.0F, 0.0F, 0.0F};
    return {
        value.x / divisor, value.y / divisor,
        value.z / divisor};
}

SyncQuaternion angleAxisSync(float angle, SyncVector axis) noexcept
{
    axis = normalizedSync(axis);
    if (lengthSync(axis) <= 0.000001F)
        return {};
    const float sine = std::sin(angle * 0.5F);
    return normalizedSync({
        axis.x * sine, axis.y * sine, axis.z * sine,
        std::cos(angle * 0.5F)});
}

float shortestSignedAngle(float angle) noexcept
{
    const float magnitude = std::abs(angle);
    if (magnitude <= syncPi)
        return angle;
    return (2.0F * syncPi - magnitude) *
           (angle > 0.0F ? -1.0F : 1.0F);
}

} // namespace

const char* BehaviorTypeName(BehaviorType value) noexcept
{
    const auto index = static_cast<std::size_t>(value);
    return index < behaviorTypeNames.size()
        ? behaviorTypeNames[index]
        : "btTouchDeath";
}

Behavior::Behavior(Behaviors* owner) noexcept : owner_(owner) {}

void Behavior::Remove() noexcept { removed_ = true; }
bool Behavior::IsRemoved() const noexcept { return removed_; }
bool Behavior::GetPhysicsNotify(
    BehaviorPhysicsNotify notify) const noexcept
{
    const auto index = static_cast<std::size_t>(notify);
    return index < physicsNotifies_.size() && physicsNotifies_[index];
}
void Behavior::SetPhysicsNotify(
    BehaviorPhysicsNotify notify, bool value) noexcept
{
    const auto index = static_cast<std::size_t>(notify);
    if (index < physicsNotifies_.size())
        physicsNotifies_[index] = value;
}
Behaviors* Behavior::GetOwner() noexcept { return owner_; }
const Behaviors* Behavior::GetOwner() const noexcept { return owner_; }
GameObject* Behavior::GetGameObj() noexcept
{
    return owner_ != nullptr ? owner_->GetGameObj() : nullptr;
}
const GameObject* Behavior::GetGameObj() const noexcept
{
    return owner_ != nullptr ? owner_->GetGameObj() : nullptr;
}
Logic* Behavior::GetLogic() noexcept
{
    auto* object = GetGameObj();
    return object != nullptr ? object->GetLogic() : nullptr;
}
const Logic* Behavior::GetLogic() const noexcept
{
    const auto* object = GetGameObj();
    return object != nullptr ? object->GetLogic() : nullptr;
}

Behaviors::Behaviors(GameObject* gameObject) noexcept
    : gameObject_(gameObject)
{
}

Behaviors::~Behaviors() { Clear(); }

Behavior& Behaviors::Add(
    BehaviorType type, std::unique_ptr<Behavior> value)
{
    if (value == nullptr || value->GetOwner() != this)
        throw std::invalid_argument("Behavior owner mismatch");
    if (gameObject_ == nullptr)
        throw std::invalid_argument("Behavior listener insertion failed");
    entries_.push_back({type, std::move(value)});
    auto& result = *entries_.back().value;
    if (!gameObject_->InsertListener(&result))
    {
        entries_.pop_back();
        throw std::invalid_argument("Behavior listener insertion failed");
    }
    return result;
}

Behavior* Behaviors::Find(BehaviorType type) noexcept
{
    return const_cast<Behavior*>(
        static_cast<const Behaviors*>(this)->Find(type));
}

const Behavior* Behaviors::Find(BehaviorType type) const noexcept
{
    const auto found = std::find_if(
        entries_.begin(), entries_.end(),
        [&](const Entry& entry) { return entry.type == type; });
    return found != entries_.end() ? found->value.get() : nullptr;
}

bool Behaviors::RemoveAt(std::size_t index) noexcept
{
    if (index >= entries_.size())
        return false;
    if (gameObject_ != nullptr && entries_[index].value != nullptr)
        gameObject_->RemoveListener(entries_[index].value.get());
    entries_.erase(
        entries_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool Behaviors::Delete(Behavior* value) noexcept
{
    const auto found = std::find_if(
        entries_.begin(), entries_.end(),
        [&](const Entry& entry) { return entry.value.get() == value; });
    return found != entries_.end() &&
           RemoveAt(static_cast<std::size_t>(
               std::distance(entries_.begin(), found)));
}

void Behaviors::Clear() noexcept
{
    while (!entries_.empty())
        RemoveAt(entries_.size() - 1U);
}

std::size_t Behaviors::GetCount() const noexcept
{
    return entries_.size();
}

bool Behaviors::RequiresPhysicsNotify(
    BehaviorPhysicsNotify notify) const noexcept
{
    return std::any_of(
        entries_.begin(), entries_.end(),
        [notify](const Entry& entry) {
            return entry.value != nullptr &&
                   entry.value->GetPhysicsNotify(notify);
        });
}

Behaviors::ProgressResult Behaviors::OnProgress(
    float deltaTime) noexcept
{
    ProgressResult result;
    for (std::size_t index = 0U; index < entries_.size();)
    {
        auto* behavior = entries_[index].value.get();
        if (behavior == nullptr || behavior->IsRemoved())
        {
            result.removed += RemoveAt(index) ? 1U : 0U;
            continue;
        }
        ++result.progressed;
        behavior->OnProgress(deltaTime);
        ++index;
    }
    return result;
}

void Behaviors::OnShot(
    const std::array<float, 3U>& position) noexcept
{
    for (auto& entry : entries_)
        if (entry.value != nullptr)
            entry.value->OnShot(position);
}

void Behaviors::OnMotor(float deltaTime, float rpm,
                        float minimumRpm, float maximumRpm) noexcept
{
    for (auto& entry : entries_)
        if (entry.value != nullptr)
            entry.value->OnMotor(
                deltaTime, rpm, minimumRpm, maximumRpm);
}

void Behaviors::OnImmortalStatus(bool status) noexcept
{
    for (auto& entry : entries_)
        if (entry.value != nullptr)
            entry.value->OnImmortalStatus(status);
}

GameObject* Behaviors::GetGameObj() noexcept { return gameObject_; }
const GameObject* Behaviors::GetGameObj() const noexcept
{
    return gameObject_;
}

GameObject::GameObject()
    : includeList_(new IncludeList(this)),
      behaviors_(new Behaviors(this)),
      frameSync_(std::make_unique<GameObjectFrameSync>())
{
}

GameObject::~GameObject()
{
    SetSyncFrameEvent(false);
    SetBodyProgressEvent(false);
    DestroyObject();
    if (behaviors_ != nullptr)
        behaviors_->Clear();
    delete behaviors_;
    if (includeList_ != nullptr)
        includeList_->Clear();
    ClearChildren();
    ClearListenerList();
    SetLogic(nullptr);
    SetParent(nullptr);
    delete includeList_;
}

GameObject::GameObject(const GameObject& other)
    : GameObject()
{
    *this = other;
}

GameObject& GameObject::operator=(const GameObject& other) noexcept
{
    if (this == &other)
        return *this;
    life = other.life;
    maximumLife = other.maximumLife;
    timeLife = other.timeLife;
    maximumTimeLife = other.maximumTimeLife;
    shieldSeconds = other.shieldSeconds;
    touchAttacker = other.touchAttacker;
    touchAttributionSeconds = other.touchAttributionSeconds;
    immortalFlag = other.immortalFlag;
    destroyed = other.destroyed;
    name_ = other.name_;
    SetLogic(other.logic_);
    objectDestroyed_ = other.objectDestroyed_;
    position_ = other.position_;
    scale_ = other.scale_;
    rotation_ = other.rotation_;
    if (frameSync_ != nullptr && other.frameSync_ != nullptr)
        *frameSync_ = *other.frameSync_;
    // GameObject::Assign does not copy the legacy listener container. Its
    // entries point at behaviors owned by the concrete source object.
    if (behaviors_ != nullptr)
        behaviors_->Clear();
    listeners_.clear();
    return *this;
}

GameObject::GameObject(GameObject&& other)
    : GameObject()
{
    *this = other;
}

GameObject& GameObject::operator=(GameObject&& other) noexcept
{
    return *this = static_cast<const GameObject&>(other);
}

void GameObject::AssignSource(GameObject& value) noexcept
{
    SetLogic(value.GetLogic());
}

const std::string& GameObject::GetName() const noexcept { return name_; }
void GameObject::SetName(std::string value) { name_ = std::move(value); }
const GameObject::Vector3& GameObject::GetPos() const noexcept
{
    return position_;
}
void GameObject::SetPos(Vector3 value) noexcept
{
    position_ = value;
}
const GameObject::Vector3& GameObject::GetScale() const noexcept
{
    return scale_;
}
void GameObject::SetScale(Vector3 value) noexcept
{
    scale_ = value;
}
const GameObject::Quaternion& GameObject::GetRot() const noexcept
{
    return rotation_;
}
void GameObject::SetRot(Quaternion value) noexcept
{
    rotation_ = value;
}

GameObject::Vector3 GameObject::GetWorldPos() const noexcept
{
    if (parent_ == nullptr)
        return position_;
    const auto parentScale = parent_->GetWorldScale();
    const Vector3 scaled{
        position_[0] * parentScale[0],
        position_[1] * parentScale[1],
        position_[2] * parentScale[2]};
    const auto rotated = rotateProxy(parent_->GetWorldRot(), scaled);
    const auto parentPosition = parent_->GetWorldPos();
    return {parentPosition[0] + rotated[0],
            parentPosition[1] + rotated[1],
            parentPosition[2] + rotated[2]};
}

void GameObject::SetWorldPos(Vector3 value) noexcept
{
    if (parent_ == nullptr)
    {
        SetPos(value);
        return;
    }
    const auto parentPosition = parent_->GetWorldPos();
    const auto parentScale = parent_->GetWorldScale();
    auto local = rotateProxy(
        inverseProxy(parent_->GetWorldRot()),
        {value[0] - parentPosition[0],
         value[1] - parentPosition[1],
         value[2] - parentPosition[2]});
    for (std::size_t axis = 0U; axis < local.size(); ++axis)
    {
        if (std::abs(parentScale[axis]) > 0.000001F)
            local[axis] /= parentScale[axis];
        else
            local[axis] = 0.0F;
    }
    SetPos(local);
}

GameObject::Vector3 GameObject::GetWorldScale() const noexcept
{
    if (parent_ == nullptr)
        return scale_;
    const auto parentScale = parent_->GetWorldScale();
    return {scale_[0] * parentScale[0],
            scale_[1] * parentScale[1],
            scale_[2] * parentScale[2]};
}

GameObject::Quaternion GameObject::GetWorldRot() const noexcept
{
    return parent_ == nullptr
        ? rotation_
        : multiplyProxy(parent_->GetWorldRot(), rotation_);
}

void GameObject::SetWorldRot(Quaternion value) noexcept
{
    SetRot(parent_ == nullptr
               ? value
               : multiplyProxy(
                     inverseProxy(parent_->GetWorldRot()), value));
}

void GameObject::CopyProxyStateFrom(const GameObject& value) noexcept
{
    position_ = value.position_;
    scale_ = value.scale_;
    rotation_ = value.rotation_;
    life = value.life;
    maximumTimeLife = value.maximumTimeLife;
    timeLife = value.timeLife;
}

MapObj* GameObject::GetMapObj() noexcept { return mapObj_; }
const MapObj* GameObject::GetMapObj() const noexcept { return mapObj_; }
void GameObject::SetMapObj(MapObj* value) noexcept { mapObj_ = value; }
Logic* GameObject::GetLogic() noexcept { return logic_; }
const Logic* GameObject::GetLogic() const noexcept { return logic_; }

void GameObject::RegFrameEvent()
{
    if (++frameEventCount_ == 1U && logic_ != nullptr)
        logic_->RegFrameEvent(this);
}

void GameObject::UnregFrameEvent() noexcept
{
    if (frameEventCount_ == 0U)
        return;
    if (--frameEventCount_ == 0U && logic_ != nullptr)
        logic_->UnregFrameEvent(this);
}

void GameObject::RegProgressEvent() noexcept
{
    // The two Logic::RegProgressEvent calls are commented out in the
    // original GameObject.cpp. Retain the counter without inventing a World
    // registration which the Windows game never made.
    ++progressEventCount_;
}

void GameObject::UnregProgressEvent() noexcept
{
    if (progressEventCount_ != 0U)
        --progressEventCount_;
}

void GameObject::RegLateProgressEvent()
{
    if (++lateProgressEventCount_ == 1U && logic_ != nullptr)
        logic_->RegLateProgressEvent(this);
}

void GameObject::UnregLateProgressEvent() noexcept
{
    if (lateProgressEventCount_ == 0U)
        return;
    if (--lateProgressEventCount_ == 0U && logic_ != nullptr)
        logic_->UnregLateProgressEvent(this);
}

void GameObject::RegFixedStepEvent()
{
    if (++fixedStepEventCount_ == 1U && logic_ != nullptr)
        logic_->RegFixedStepEvent(this);
}

void GameObject::UnregFixedStepEvent() noexcept
{
    if (fixedStepEventCount_ == 0U)
        return;
    if (--fixedStepEventCount_ == 0U && logic_ != nullptr)
        logic_->UnregFixedStepEvent(this);
}

void GameObject::SetSyncFrameEvent(bool value)
{
    if (syncFrameEvent_ == value)
        return;
    syncFrameEvent_ = value;
    if (value)
        RegFrameEvent();
    else
        UnregFrameEvent();
}

void GameObject::SetBodyProgressEvent(bool value)
{
    if (bodyProgressEvent_ == value)
        return;
    bodyProgressEvent_ = value;
    if (value)
    {
        RegLateProgressEvent();
        RegFrameEvent();
    }
    else
    {
        UnregLateProgressEvent();
        UnregFrameEvent();
    }
}

void GameObject::SetLogic(Logic* value) noexcept
{
    if (logic_ == value)
        return;
    if (logic_ != nullptr)
    {
        if (frameEventCount_ > 0U)
            logic_->UnregFrameEvent(this);
        // Original GameObject.cpp deliberately does not register progress.
        if (lateProgressEventCount_ > 0U)
            logic_->UnregLateProgressEvent(this);
        if (fixedStepEventCount_ > 0U)
            logic_->UnregFixedStepEvent(this);
        LogicReleased();
    }
    logic_ = value;
    for (auto* child : children_)
    {
        if (child != nullptr)
            child->SetLogic(value);
    }
    if (logic_ != nullptr)
    {
        if (frameEventCount_ > 0U)
            logic_->RegFrameEvent(this);
        if (lateProgressEventCount_ > 0U)
            logic_->RegLateProgressEvent(this);
        if (fixedStepEventCount_ > 0U)
            logic_->RegFixedStepEvent(this);
        LogicInited();
    }
}

unsigned GameObject::GetFrameEventCount() const noexcept
{
    return frameEventCount_;
}

unsigned GameObject::GetProgressEventCount() const noexcept
{
    return progressEventCount_;
}

unsigned GameObject::GetLateProgressEventCount() const noexcept
{
    return lateProgressEventCount_;
}

unsigned GameObject::GetFixedStepEventCount() const noexcept
{
    return fixedStepEventCount_;
}

bool GameObject::IsSyncFrameEvent() const noexcept
{
    return syncFrameEvent_;
}

bool GameObject::IsBodyProgressEvent() const noexcept
{
    return bodyProgressEvent_;
}

void GameObject::OnFixedStep(float deltaTime) noexcept
{
    static_cast<void>(deltaTime);
}

void GameObject::OnLateProgress(
    float deltaTime, bool physicsStep) noexcept
{
    static_cast<void>(deltaTime);
    static_cast<void>(physicsStep);
}

void GameObject::OnFrame(
    float deltaTime, float physicsAlpha) noexcept
{
    static_cast<void>(deltaTime);
    static_cast<void>(physicsAlpha);
}

Proj* GameObject::IsProj() noexcept { return nullptr; }
const Proj* GameObject::IsProj() const noexcept { return nullptr; }
GameCar* GameObject::IsCar() noexcept { return nullptr; }
const GameCar* GameObject::IsCar() const noexcept { return nullptr; }

void GameObject::InsertChild(GameObject* value)
{
    if (value == nullptr || value == this || value->parent_ != nullptr)
        return;
    value->parent_ = this;
    children_.push_back(value);
    value->SetLogic(logic_);
}

void GameObject::RemoveChild(GameObject* value) noexcept
{
    if (value == nullptr || value->parent_ != this)
        return;
    value->parent_ = nullptr;
    const auto found = std::find(children_.begin(), children_.end(), value);
    if (found != children_.end())
        children_.erase(found);
}

void GameObject::ClearChildren() noexcept
{
    while (!children_.empty())
        RemoveChild(children_.front());
}

GameObject* GameObject::GetParent() noexcept { return parent_; }
const GameObject* GameObject::GetParent() const noexcept { return parent_; }

void GameObject::SetParent(GameObject* value)
{
    if (parent_ == value || value == this)
        return;
    if (parent_ != nullptr)
        parent_->RemoveChild(this);
    if (value != nullptr)
        value->InsertChild(this);
}

const GameObject::Children& GameObject::GetChildren() const noexcept
{
    return children_;
}

GameObject::IncludeList& GameObject::GetIncludeList() noexcept
{
    return *includeList_;
}

const GameObject::IncludeList& GameObject::GetIncludeList() const noexcept
{
    return *includeList_;
}

Behaviors& GameObject::GetBehaviors() noexcept { return *behaviors_; }
const Behaviors& GameObject::GetBehaviors() const noexcept
{
    return *behaviors_;
}
GameObjectFrameSync& GameObject::GetFrameSync() noexcept
{
    return *frameSync_;
}
const GameObjectFrameSync& GameObject::GetFrameSync() const noexcept
{
    return *frameSync_;
}

void GameObject::ResetGameObject(float maximumLifeValue) noexcept
{
    maximumLife = maximumLifeValue;
    life = maximumLife;
    timeLife = 0.0F;
    maximumTimeLife = -1.0F;
    shieldSeconds = 0.0F;
    touchAttacker = undefinedPlayerId;
    touchAttributionSeconds = 0.0F;
    immortalFlag = false;
    destroyed = false;
    frameSync_->Reset();
    objectDestroyed_ = false;
}

GameObject::ProgressResult GameObject::OnProgress(
    float deltaTime, bool allowLifetimeDeath) noexcept
{
    ProgressResult result;
    timeLife += deltaTime;
    if (shieldSeconds > 0.0F)
    {
        shieldSeconds -= deltaTime;
        if (shieldSeconds <= 0.0F)
        {
            shieldSeconds = 0.0F;
            result.immortalityEnded = true;
            OnImmortalStatusEvent(false);
            behaviors_->OnImmortalStatus(false);
        }
    }
    const auto included = includeList_->OnProgress(deltaTime);
    result.includedProgressed = included.progressed;
    result.includedRemoved = included.removed;
    if (touchAttributionSeconds > 0.0F &&
        (touchAttributionSeconds -= deltaTime) <= 0.0F)
    {
        touchAttacker = undefinedPlayerId;
        touchAttributionSeconds = 0.0F;
        result.touchAttributionEnded = true;
    }
    if (allowLifetimeDeath && maximumTimeLife > 0.0F &&
        timeLife > maximumTimeLife)
        result.lifetimeDeath = Death();
    const auto behaviors = behaviors_->OnProgress(deltaTime);
    result.behaviorsProgressed = behaviors.progressed;
    result.behaviorsRemoved = behaviors.removed;
    return result;
}

GameObject::DamageResult GameObject::Damage(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    const bool immortal = IsImmortal();
    const float newLife = immortal ? life : life - value;
    return Damage(senderPlayerId, value, newLife,
                  !immortal && newLife <= 0.0F, damageType);
}

GameObject::DamageResult GameObject::Damage(
    std::size_t senderPlayerId, float value, float newLife,
    bool death, DamageType damageType) noexcept
{
    DamageResult result;
    result.previousLife = life;
    result.newLife = newLife;
    result.damage = value;
    result.damageType = damageType;

    // GameObject.cpp assigns authoritative life before checking lsDeath.
    life = newLife;
    result.wasLive = !destroyed;
    if (!result.wasLive)
        return result;

    // GameObject::Damage dispatches listeners immediately after assigning
    // authoritative life, before touch attribution and death events.
    OnDamageEvent(value, damageType);
    const auto damageListeners = listeners_;
    for (auto* listener : damageListeners)
    {
        if (listener != nullptr &&
            std::find(listeners_.begin(), listeners_.end(), listener) !=
                listeners_.end())
            listener->OnDamage(*this, value, damageType);
    }

    if (senderPlayerId != undefinedPlayerId &&
        damageType == DamageType::Touch)
    {
        touchAttacker = senderPlayerId;
        touchAttributionSeconds = 3.0F;
    }
    // Source GameObject::Damage emits cPlayerDamage for every mortal object,
    // including absorbed damage, before testing the authoritative death bit.
    if (maximumLife >= 0.0F)
        OnDamageDispatchEvent(senderPlayerId, value, damageType);
    if (!death)
        return result;

    if (damageType != DamageType::Touch)
    {
        touchAttacker = undefinedPlayerId;
        touchAttributionSeconds = 0.0F;
    }
    destroyed = true;
    result.death = true;
    result.killCredit = IsCar() != nullptr &&
                        senderPlayerId != undefinedPlayerId &&
                        damageType != DamageType::Mine;
    if (result.killCredit)
        OnKillDispatchEvent(senderPlayerId, value, damageType);
    SendDeath(damageType, nullptr);
    return result;
}

bool GameObject::Death(
    DamageType damageType, GameObject* target) noexcept
{
    if (destroyed)
        return false;
    // Direct Death does not clear _touchPlayerId in the Windows source.
    // TouchDeath relies on that distinction so an earlier car contact can
    // still be attributed when the victim crosses the death plane.
    destroyed = true;
    SendDeath(damageType, target);
    return true;
}

bool GameObject::Resc() noexcept
{
    if (!destroyed)
        return false;
    destroyed = false;
    timeLife = 0.0F;
    life = maximumLife;
    return true;
}

void GameObject::Healt(float value) noexcept
{
    life = std::min(life + value, maximumLife);
}

void GameObject::LowLife(Behavior* behavior) noexcept
{
    OnLowLifeEvent();
    const auto lowLifeListeners = listeners_;
    for (auto* listener : lowLifeListeners)
    {
        if (listener != nullptr &&
            std::find(listeners_.begin(), listeners_.end(), listener) !=
                listeners_.end())
            listener->OnLowLife(*this, behavior);
    }
}

void GameObject::OnContact(GameObject* target) noexcept
{
    const auto contactListeners = listeners_;
    for (auto* listener : contactListeners)
    {
        if (listener != nullptr &&
            std::find(listeners_.begin(), listeners_.end(), listener) !=
                listeners_.end())
            listener->OnContact(*this, target);
    }
}

bool GameObject::InsertListener(GameObjectListener* value) noexcept
{
    if (value == nullptr ||
        std::find(listeners_.begin(), listeners_.end(), value) !=
            listeners_.end())
    {
        return false;
    }
    listeners_.push_back(value);
    return true;
}

bool GameObject::RemoveListener(GameObjectListener* value) noexcept
{
    const auto found = std::find(
        listeners_.begin(), listeners_.end(), value);
    if (found == listeners_.end())
        return false;
    listeners_.erase(found);
    return true;
}

void GameObject::ClearListenerList() noexcept
{
    listeners_.clear();
}

std::size_t GameObject::GetListenerCount() const noexcept
{
    return listeners_.size();
}

bool GameObject::DestroyObject() noexcept
{
    if (objectDestroyed_)
        return false;
    objectDestroyed_ = true;
    OnDestroyEvent();
    const auto destroyListeners = listeners_;
    for (auto* listener : destroyListeners)
    {
        // A listener callback may release a child behavior and remove its
        // listener later in this snapshot. The Windows ref-counted listener
        // list never dispatches an entry after that removal.
        if (listener != nullptr &&
            std::find(listeners_.begin(), listeners_.end(), listener) !=
                listeners_.end())
            listener->OnDestroy(*this);
    }
    return true;
}

bool GameObject::IsObjectDestroyed() const noexcept
{
    return objectDestroyed_;
}

void GameObject::SendDeath(
    DamageType damageType, GameObject* target) noexcept
{
    OnDeathEvent(damageType, target);
    const auto deathListeners = listeners_;
    for (auto* listener : deathListeners)
    {
        if (listener != nullptr &&
            std::find(listeners_.begin(), listeners_.end(), listener) !=
                listeners_.end())
            listener->OnDeath(*this, damageType, target);
    }
}

void GameObject::SetImmortalFlag(bool value) noexcept
{
    immortalFlag = value;
}
bool GameObject::GetImmortalFlag() const noexcept { return immortalFlag; }
void GameObject::Immortal(float time) noexcept
{
    const bool turnOn = shieldSeconds <= 0.0F && time > 0.0F;
    shieldSeconds = time;
    if (turnOn)
    {
        OnImmortalStatusEvent(true);
        behaviors_->OnImmortalStatus(true);
    }
}
bool GameObject::IsImmortal() const noexcept
{
    return maximumLife < 0.0F || shieldSeconds > 0.0F || immortalFlag;
}
bool GameObject::IsTimedImmortal() const noexcept
{
    return shieldSeconds > 0.0F;
}

GameObject::LiveState GameObject::GetLiveState() const noexcept
{
    return destroyed ? LiveState::Death : LiveState::Live;
}
float GameObject::GetMaxLife() const noexcept { return maximumLife; }
void GameObject::SetMaxLife(float value) noexcept
{
    life = maximumLife = value;
}
float GameObject::GetLife() const noexcept { return life; }
void GameObject::SetLife(float value) noexcept { life = value; }
float GameObject::GetTimeLife() const noexcept { return timeLife; }
void GameObject::SetTimeLife(float value) noexcept { timeLife = value; }
float GameObject::GetMaxTimeLife() const noexcept
{
    return maximumTimeLife;
}
void GameObject::SetMaxTimeLife(float value) noexcept
{
    maximumTimeLife = value;
}
std::size_t GameObject::GetTouchPlayerId() const noexcept
{
    return touchAttacker;
}

void GameObjectFrameSync::Reset() noexcept
{
    *this = {};
}

void GameObjectFrameSync::SetPosSync(Vector value) noexcept
{
    posSync_ = value;
    posSyncDirection_ = normalizedSync(value);
    posSyncLength_ = lengthSync(value);
}

void GameObjectFrameSync::SetRotSync(Quaternion value) noexcept
{
    rotSync_ = normalizedSync(value);
    rotSyncAxis_ = axisSync(rotSync_);
    rotSyncAngle_ = shortestSignedAngle(angleSync(rotSync_));
}

void GameObjectFrameSync::SetPosSync2(
    Vector current, Vector next) noexcept
{
    posSyncDirection2_ = subtractSync(current, next);
    posSyncDistance2_ = lengthSync(posSyncDirection2_);
    posSyncDirection2_ = normalizedSync(posSyncDirection2_);
    posSync2_ = next;
    posSyncLength2_ = lengthSync(posSync2_);
}

void GameObjectFrameSync::SetRotSync2(
    Quaternion current, Quaternion next) noexcept
{
    const Quaternion difference = rotationSync(next, current);
    rotSyncAxis2_ = axisSync(difference);
    rotSyncAngle2_ = shortestSignedAngle(angleSync(difference));
    rotSync2_ = normalizedSync(next);
    rotSyncLength2_ = angleSync(rotSync2_);
}

void GameObjectFrameSync::OnPhysicsState(
    Pose pose, Vector linearVelocity, bool awake) noexcept
{
    pose.rotation = normalizedSync(pose.rotation);
    if (!physicsStateInitialized_)
    {
        previousPhysicsPose_ = pose;
        currentPhysicsPose_ = pose;
        renderPhysicsPose_ = pose;
        previousPhysicsVelocity_ = linearVelocity;
        currentPhysicsVelocity_ = linearVelocity;
        renderPhysicsVelocity_ = linearVelocity;
        physicsStateInitialized_ = true;
        bodyProgressEvent_ = awake;
        return;
    }

    previousPhysicsPose_ = currentPhysicsPose_;
    currentPhysicsPose_ = pose;
    previousPhysicsVelocity_ = currentPhysicsVelocity_;
    currentPhysicsVelocity_ = linearVelocity;
    if (bodyProgressEvent_ && !awake)
    {
        // OnSleep unregisters future body progress only after the actor's
        // final solved pose has reached the graph actor.
        renderPhysicsPose_ = currentPhysicsPose_;
        renderPhysicsVelocity_ = currentPhysicsVelocity_;
    }
    bodyProgressEvent_ = awake;
}

GameObjectFrameSync::NetworkCorrection
GameObjectFrameSync::OnNetworkPose(
    Vector physicsPosition, Vector graphPosition,
    Quaternion graphRotation, Vector targetPosition,
    Quaternion targetRotation) noexcept
{
    NetworkCorrection result;
    const Vector positionDifference =
        subtractSync(targetPosition, physicsPosition);
    if (lengthSync(positionDifference) > 4.0F)
    {
        SetPosSync(subtractSync(targetPosition, graphPosition));
        result.snapPosition = true;
    }

    const Quaternion rotationDifference =
        rotationSync(graphRotation, targetRotation);
    const float rotationAngle = std::abs(
        shortestSignedAngle(angleSync(rotationDifference)));
    if (rotationAngle > syncPi / 24.0F)
    {
        SetRotSync(rotationDifference);
        result.snapRotation = true;
    }
    return result;
}

GameObjectFrameSync::Pose GameObjectFrameSync::OnFrame(
    Pose physicsPose, float deltaTime, float physicsAlpha) noexcept
{
    deltaTime = std::max(deltaTime, 0.0F);
    Pose result = physicsPose;
    if (physicsStateInitialized_)
    {
        if (bodyProgressEvent_)
        {
            result.position = lerpSync(
                previousPhysicsPose_.position,
                currentPhysicsPose_.position, physicsAlpha);
            result.rotation = slerpSync(
                previousPhysicsPose_.rotation,
                currentPhysicsPose_.rotation, physicsAlpha);
            renderPhysicsVelocity_ = lerpSync(
                previousPhysicsVelocity_, currentPhysicsVelocity_,
                physicsAlpha);
            renderPhysicsPose_ = result;
        }
        else
        {
            result = renderPhysicsPose_;
        }
    }
    if (posSyncLength_ > 0.0F && posSyncLength_ < 5.0F)
    {
        posSyncLength_ = std::max(
            posSyncLength_ - 5.0F * deltaTime, 0.0F);
        result.position.x -= posSyncDirection_.x * posSyncLength_;
        result.position.y -= posSyncDirection_.y * posSyncLength_;
        result.position.z -= posSyncDirection_.z * posSyncLength_;
    }
    else
    {
        posSyncLength_ = 0.0F;
    }

    if (rotSyncAngle_ != 0.0F)
    {
        if (rotSyncAngle_ > 0.0F)
        {
            rotSyncAngle_ = std::max(
                rotSyncAngle_ - 1.3F * syncPi * deltaTime,
                0.0F);
        }
        else
        {
            rotSyncAngle_ = std::min(
                rotSyncAngle_ + 1.3F * syncPi * deltaTime,
                0.0F);
        }
        result.rotation = multiplySync(
            angleAxisSync(-rotSyncAngle_, rotSyncAxis_),
            result.rotation);
    }

    if (posSyncDistance2_ > 0.0F && posSyncDistance2_ < 5.0F)
    {
        posSyncDistance2_ = std::max(
            posSyncDistance2_ - 5.0F * deltaTime, 0.0F);
        result.position.x +=
            posSync2_.x + posSyncDirection2_.x * posSyncDistance2_;
        result.position.y +=
            posSync2_.y + posSyncDirection2_.y * posSyncDistance2_;
        result.position.z +=
            posSync2_.z + posSyncDirection2_.z * posSyncDistance2_;
    }
    else if (posSyncLength2_ > 0.0F)
    {
        posSyncDistance2_ = 0.0F;
        result.position.x += posSync2_.x;
        result.position.y += posSync2_.y;
        result.position.z += posSync2_.z;
    }

    if (rotSyncAngle2_ != 0.0F)
    {
        if (rotSyncAngle2_ > 0.0F)
        {
            rotSyncAngle2_ = std::max(
                rotSyncAngle2_ - 1.3F * syncPi * deltaTime,
                0.0F);
        }
        else
        {
            rotSyncAngle2_ = std::min(
                rotSyncAngle2_ + 1.3F * syncPi * deltaTime,
                0.0F);
        }
        result.rotation = multiplySync(
            angleAxisSync(-rotSyncAngle2_, rotSyncAxis2_),
            multiplySync(rotSync2_, result.rotation));
    }
    else if (rotSyncLength2_ != 0.0F)
    {
        rotSyncAngle2_ = 0.0F;
        result.rotation = multiplySync(rotSync2_, result.rotation);
    }
    return result;
}

const GameObjectFrameSync::Vector&
GameObjectFrameSync::GetPosSync() const noexcept
{
    return posSync_;
}

const GameObjectFrameSync::Quaternion&
GameObjectFrameSync::GetRotSync() const noexcept
{
    return rotSync_;
}

const GameObjectFrameSync::Vector&
GameObjectFrameSync::GetPosSync2() const noexcept
{
    return posSync2_;
}

const GameObjectFrameSync::Quaternion&
GameObjectFrameSync::GetRotSync2() const noexcept
{
    return rotSync2_;
}

const GameObjectFrameSync::Vector&
GameObjectFrameSync::GetRenderVelocity() const noexcept
{
    return renderPhysicsVelocity_;
}

bool GameObjectFrameSync::IsBodyProgressEvent() const noexcept
{
    return bodyProgressEvent_;
}

bool GameObjectFrameSync::HasPhysicsState() const noexcept
{
    return physicsStateInitialized_;
}

bool GameObjectFrameSync::HasActiveCorrection() const noexcept
{
    return posSyncLength_ != 0.0F || rotSyncAngle_ != 0.0F ||
           posSyncDistance2_ != 0.0F || posSyncLength2_ != 0.0F ||
           rotSyncAngle2_ != 0.0F || rotSyncLength2_ != 0.0F;
}

DestrObj::DestrObj()
    : destructionList_(new MapObjects(this))
{
}

DestrObj::~DestrObj()
{
    if (destructionList_ != nullptr)
        destructionList_->Clear();
    delete destructionList_;
}

GameObject::DamageResult DestrObj::Damage(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    auto result = GameObject::Damage(
        senderPlayerId, value, damageType);
    checkDestruction_ = checkDestruction_ || result.death;
    return result;
}

GameObject::DamageResult DestrObj::Damage(
    std::size_t senderPlayerId, float value, float newLife,
    bool death, DamageType damageType) noexcept
{
    auto result = GameObject::Damage(
        senderPlayerId, value, newLife, death, damageType);
    checkDestruction_ = checkDestruction_ || result.death;
    return result;
}

bool DestrObj::Death(
    DamageType damageType, GameObject* target) noexcept
{
    const bool died = GameObject::Death(damageType, target);
    checkDestruction_ = checkDestruction_ || died;
    return died;
}

bool DestrObj::OnProgress(float deltaTime) noexcept
{
    GameObject::OnProgress(deltaTime);
    destructionList_->OnProgress(deltaTime);
    if (!checkDestruction_)
        return false;
    checkDestruction_ = false;
    return true;
}

bool DestrObj::HasPendingDestruction() const noexcept
{
    return checkDestruction_;
}

MapObjects& DestrObj::GetDestrList() noexcept
{
    return *destructionList_;
}

const MapObjects& DestrObj::GetDestrList() const noexcept
{
    return *destructionList_;
}

std::size_t DestrObj::ReleaseDestruction(Map& map)
{
    const auto position = GetPos();
    const auto rotation = GetRot();
    std::size_t released = 0U;
    for (std::size_t slot = 0U;
         slot < destructionList_->GetSlotCount(); ++slot)
    {
        auto* child = destructionList_->Get(slot);
        if (child == nullptr)
            continue;
        auto detached = destructionList_->Extract(child);
        if (detached == nullptr)
            continue;
        detached->SetName({});
        auto& inserted = map.InsertMapObj(std::move(detached));
        inserted.GetGameObj().SetPos(position);
        inserted.GetGameObj().SetRot(rotation);
        ++released;
    }
    destructionList_->Clear();
    return released;
}

TouchDeath::TouchDeath(Behaviors* owner) noexcept
    : Behavior(owner)
{
    SetPhysicsNotify(BehaviorPhysicsNotify::Contact, true);
}

void TouchDeath::OnProgress(float) noexcept {}

void TouchDeath::OnContact(
    GameObject&, GameObject* target) noexcept
{
    if (target != nullptr)
        target->Death(DamageType::DeathPlane);
}

ResurrectObj::ResurrectObj(Behaviors* owner) noexcept
    : Behavior(owner)
{
}

void ResurrectObj::OnProgress(float) noexcept {}

void ResurrectObj::OnDeath(
    GameObject& sender, DamageType, GameObject*) noexcept
{
    Resurrect(sender);
}

bool ResurrectObj::Resurrect(GameObject& owner) noexcept
{
    if (resurrect_)
        return false;
    resurrect_ = true;
    owner.Resc();
    auto* mapObject = owner.GetMapObj();
    auto* collection = mapObject != nullptr
        ? mapObject->GetOwner()
        : nullptr;
    if (collection == nullptr || collection->GetOwner() == nullptr)
        return true;

    auto* logic = owner.GetLogic();
    auto* map = logic != nullptr ? logic->GetMap() : nullptr;
    if (map == nullptr)
        return true;

    const auto worldPosition = owner.GetWorldPos();
    const auto worldRotation = owner.GetWorldRot();
    auto detached = collection->Extract(mapObject);
    if (detached == nullptr)
        return true;
    detached->SetName({});
    owner.SetWorldPos(worldPosition);
    owner.SetWorldRot(worldRotation);
    map->InsertMapObj(std::move(detached));
    return true;
}

bool ResurrectObj::IsResurrect() const noexcept
{
    return resurrect_;
}

FxSystemWaitingEnd::FxSystemWaitingEnd(
    Behaviors* owner) noexcept
    : ResurrectObj(owner)
{
}

void FxSystemWaitingEnd::OnProgress(float) noexcept
{
    auto* owner = GetGameObj();
    if (owner == nullptr || !IsResurrect() ||
        liveParticles_ != 0U || owner->destroyed)
        return;
    finalDeath_ = owner->Death() || finalDeath_;
}

void FxSystemWaitingEnd::OnDeath(
    GameObject& sender, DamageType, GameObject*) noexcept
{
    if (Resurrect(sender))
    {
        fading_ = true;
        beginFading_ = true;
    }
}

void FxSystemWaitingEnd::SetLiveParticleCount(
    std::size_t value) noexcept
{
    liveParticles_ = value;
}

bool FxSystemWaitingEnd::IsResurrect() const noexcept
{
    return ResurrectObj::IsResurrect();
}

bool FxSystemWaitingEnd::IsFading() const noexcept
{
    return fading_;
}

bool FxSystemWaitingEnd::ConsumeBeginFading() noexcept
{
    const bool result = beginFading_;
    beginFading_ = false;
    return result;
}

bool FxSystemWaitingEnd::ConsumeFinalDeath() noexcept
{
    const bool result = finalDeath_;
    finalDeath_ = false;
    return result;
}

void FxSystemSrcSpeed::Reset() noexcept
{
    sourceSpeed_ = {};
    actorVelocity_ = {};
    worldSourceSpeed_ = {};
    parent_ = {};
    actorAvailable_ = false;
    hasParent_ = false;
}

bool FxSystemSrcSpeed::OnProgress(
    bool physicsActorAvailable, Vector actorLinearVelocity,
    const ParentTransform* parent) noexcept
{
    // The Windows behavior returns before touching FxParticleSystem when the
    // GameObject does not resolve to an NxActor.  Preserve the last value in
    // that case instead of replacing it with zero.
    if (!physicsActorAvailable)
        return false;

    if (parent != nullptr)
    {
        const auto& rotation = parent->rotation;
        const float lengthSquared =
            rotation.x * rotation.x + rotation.y * rotation.y +
            rotation.z * rotation.z + rotation.w * rotation.w;
        Quaternion inverse;
        if (lengthSquared > 0.0000001F)
        {
            inverse = {-rotation.x / lengthSquared,
                       -rotation.y / lengthSquared,
                       -rotation.z / lengthSquared,
                       rotation.w / lengthSquared};
        }

        const Vector twiceCross{
            2.0F * (inverse.y * actorLinearVelocity.z -
                    inverse.z * actorLinearVelocity.y),
            2.0F * (inverse.z * actorLinearVelocity.x -
                    inverse.x * actorLinearVelocity.z),
            2.0F * (inverse.x * actorLinearVelocity.y -
                    inverse.y * actorLinearVelocity.x)};
        Vector local{
            actorLinearVelocity.x + inverse.w * twiceCross.x +
                (inverse.y * twiceCross.z - inverse.z * twiceCross.y),
            actorLinearVelocity.y + inverse.w * twiceCross.y +
                (inverse.z * twiceCross.x - inverse.x * twiceCross.z),
            actorLinearVelocity.z + inverse.w * twiceCross.z +
                (inverse.x * twiceCross.y - inverse.y * twiceCross.x)};

        // Vec3TransformNormal(GetInvWorldMat()) includes inverse scale and
        // excludes only translation.
        const auto inverseScale = [](float value, float scale) noexcept {
            return std::abs(scale) > 0.0000001F ? value / scale : 0.0F;
        };
        local.x = inverseScale(local.x, parent->scale.x);
        local.y = inverseScale(local.y, parent->scale.y);
        local.z = inverseScale(local.z, parent->scale.z);
        sourceSpeed_ = local;
    }
    else
    {
        sourceSpeed_ = actorLinearVelocity;
    }
    return true;
}

const FxSystemSrcSpeed::Vector&
FxSystemSrcSpeed::GetSourceSpeed() const noexcept
{
    return sourceSpeed_;
}

FxSystemSrcSpeed::FxSystemSrcSpeed() noexcept : Behavior(nullptr) {}

FxSystemSrcSpeed::FxSystemSrcSpeed(Behaviors* owner) noexcept
    : Behavior(owner)
{
}

void FxSystemSrcSpeed::OnProgress(float) noexcept
{
    if (!OnProgress(
            actorAvailable_, actorVelocity_,
            hasParent_ ? &parent_ : nullptr))
        return;
    // FxFlowEmitter transforms the local value back through the same owner
    // graph. Retain the actor-space result for the backend adapter so it does
    // not reconstruct another temporary behavior in every render pass.
    worldSourceSpeed_ = actorVelocity_;
}

void FxSystemSrcSpeed::SetPhysicsInput(
    bool actorAvailable, Vector velocity,
    const ParentTransform* parent) noexcept
{
    actorAvailable_ = actorAvailable;
    actorVelocity_ = velocity;
    hasParent_ = parent != nullptr;
    if (parent != nullptr)
        parent_ = *parent;
}

const FxSystemSrcSpeed::Vector&
FxSystemSrcSpeed::GetWorldSourceSpeed() const noexcept
{
    return worldSourceSpeed_;
}

bool FxSystemSrcSpeed::HasPhysicsActor() const noexcept
{
    return actorAvailable_;
}

EventEffect::EventEffect() noexcept : Behavior(nullptr) {}

EventEffect::EventEffect(Behaviors* owner) noexcept : Behavior(owner) {}

EventEffect::EventEffect(const EventEffect& other)
    : Behavior(nullptr), definition_(other.definition_), position_(other.position_),
      impulse_(other.impulse_), ignoreRotation_(other.ignoreRotation_),
      effectState_(std::make_shared<EffectState>(*other.effectState_)),
      soundPaths_(other.soundPaths_)
{
}

void EventEffect::OnProgress(float) noexcept {}

EventEffect& EventEffect::operator=(const EventEffect& other)
{
    if (this == &other)
        return *this;
    definition_ = other.definition_;
    position_ = other.position_;
    impulse_ = other.impulse_;
    ignoreRotation_ = other.ignoreRotation_;
    // Copying a source behavior creates an independent listener/effect list.
    // Replacing the shared state also expires references to the old behavior,
    // matching EventEffect::~EventEffect listener detachment on Windows.
    effectState_ = std::make_shared<EffectState>(*other.effectState_);
    soundPaths_ = other.soundPaths_;
    return *this;
}

void EventEffect::Configure(
    const ObjectDefinition* definition,
    std::array<float, 3U> position,
    std::array<float, 3U> impulse,
    bool ignoreRotation) noexcept
{
    definition_ = definition;
    position_ = position;
    impulse_ = impulse;
    ignoreRotation_ = ignoreRotation;
    Reset();
}

void EventEffect::ConfigureSounds(
    std::vector<std::string> soundPaths)
{
    soundPaths_ = std::move(soundPaths);
}

void EventEffect::Reset() noexcept
{
    effectState_->effectIds.clear();
    effectState_->makeEffectId = invalidEffect;
}

EventEffect::EffectId EventEffect::CreateEffect() noexcept
{
    EffectId effect = effectState_->nextEffectId++;
    if (effect == invalidEffect)
        effect = effectState_->nextEffectId++;
    effectState_->effectIds.push_back(effect);
    return effect;
}

bool EventEffect::MakeEffect() noexcept
{
    if (effectState_->makeEffectId != invalidEffect)
        return false;
    effectState_->makeEffectId = CreateEffect();
    return true;
}

bool EventEffect::FreeEffect() noexcept
{
    if (effectState_->makeEffectId == invalidEffect)
        return false;
    return OnDestroyEffect(effectState_->makeEffectId);
}

bool EventEffect::OnDestroyEffect() noexcept
{
    return effectState_->makeEffectId != invalidEffect &&
        OnDestroyEffect(effectState_->makeEffectId);
}

bool EventEffect::OnDestroyEffect(EffectId effect) noexcept
{
    if (effect == invalidEffect)
        return false;
    const auto found = std::find(
        effectState_->effectIds.begin(), effectState_->effectIds.end(),
        effect);
    if (found == effectState_->effectIds.end())
        return false;
    if (effectState_->makeEffectId == effect)
        effectState_->makeEffectId = invalidEffect;
    effectState_->effectIds.erase(found);
    return true;
}

bool EventEffect::IsEffectMaked() const noexcept
{
    return effectState_->makeEffectId != invalidEffect;
}

EventEffect::EffectId EventEffect::GetMakeEffectId() const noexcept
{
    return effectState_->makeEffectId;
}

std::size_t EventEffect::GetEffectCount() const noexcept
{
    return effectState_->effectIds.size();
}

bool EventEffect::HasEffect(EffectId effect) const noexcept
{
    return std::find(
               effectState_->effectIds.begin(),
               effectState_->effectIds.end(), effect) !=
        effectState_->effectIds.end();
}

EventEffect::EffectReference EventEffect::ObserveEffect(
    EffectId effect) noexcept
{
    return EffectReference(effectState_, effect);
}

EventEffect::EffectReference::EffectReference(
    std::weak_ptr<EffectState> state, EffectId effect) noexcept
    : state_(std::move(state)), effect_(effect)
{
}

bool EventEffect::EffectReference::HasOwner() const noexcept
{
    return !state_.expired();
}

bool EventEffect::EffectReference::HasEffect() const noexcept
{
    const auto state = state_.lock();
    return state != nullptr && effect_ != invalidEffect &&
        std::find(
            state->effectIds.begin(), state->effectIds.end(), effect_) !=
            state->effectIds.end();
}

bool EventEffect::EffectReference::IsEffectMaked() const noexcept
{
    const auto state = state_.lock();
    return state != nullptr &&
        state->makeEffectId != invalidEffect;
}

bool EventEffect::EffectReference::IsOwnedBy(
    const EventEffect& owner) const noexcept
{
    const auto state = state_.lock();
    return state != nullptr && state == owner.effectState_;
}

bool EventEffect::EffectReference::NotifyDestroyed() noexcept
{
    const auto state = state_.lock();
    const EffectId effect = effect_;
    Reset();
    if (state == nullptr || effect == invalidEffect)
        return false;
    const auto found = std::find(
        state->effectIds.begin(), state->effectIds.end(), effect);
    if (found == state->effectIds.end())
        return false;
    if (state->makeEffectId == effect)
        state->makeEffectId = invalidEffect;
    state->effectIds.erase(found);
    return true;
}

EventEffect::EffectId
EventEffect::EffectReference::GetEffectId() const noexcept
{
    return effect_;
}

void EventEffect::EffectReference::Reset() noexcept
{
    state_.reset();
    effect_ = invalidEffect;
}

EventEffect::SpawnResult EventEffect::GetSpawnResult(
    bool created) noexcept
{
    return {
        created && definition_ != nullptr,
        true,
        this,
        created ? effectState_->makeEffectId : invalidEffect,
        definition_,
        position_,
        impulse_,
        ignoreRotation_};
}

const ObjectDefinition* EventEffect::GetEffectDefinition() const noexcept
{
    return definition_;
}

const std::array<float, 3U>& EventEffect::GetPosition() const noexcept
{
    return position_;
}

const std::array<float, 3U>& EventEffect::GetImpulse() const noexcept
{
    return impulse_;
}

bool EventEffect::GetIgnoreRotation() const noexcept
{
    return ignoreRotation_;
}

const std::vector<std::string>&
EventEffect::GetSoundPaths() const noexcept
{
    return soundPaths_;
}

const std::string* EventEffect::SelectSoundPath(
    float randomUnit) const noexcept
{
    if (soundPaths_.empty())
        return nullptr;
    const float normalized = std::isfinite(randomUnit)
        ? std::clamp(
              randomUnit, 0.0F,
              std::nextafter(1.0F, 0.0F))
        : 0.0F;
    const auto index = std::min(
        soundPaths_.size() - 1U,
        static_cast<std::size_t>(
            normalized * static_cast<float>(soundPaths_.size())));
    return &soundPaths_[index];
}

DeathEffect::DeathEffect() noexcept : EventEffect() {}

DeathEffect::DeathEffect(bool effectPhysicsIgnoreSenderCar,
                         bool targetChild) noexcept
    : EventEffect()
{
    Reset(effectPhysicsIgnoreSenderCar, targetChild);
}

DeathEffect::DeathEffect(
    Behaviors* owner, bool effectPhysicsIgnoreSenderCar,
    bool targetChild) noexcept
    : EventEffect(owner)
{
    Reset(effectPhysicsIgnoreSenderCar, targetChild);
}

void DeathEffect::OnProgress(float) noexcept {}

void DeathEffect::Reset(bool effectPhysicsIgnoreSenderCar,
                        bool targetChild) noexcept
{
    EventEffect::Reset();
    effectPhysicsIgnoreSenderCar_ = effectPhysicsIgnoreSenderCar;
    targetChild_ = targetChild;
    pending_ = {};
    logicAvailable_ = false;
    senderIsWeaponProjectile_ = false;
}

void DeathEffect::ConfigureSource(
    const ObjectDefinition* definition,
    std::array<float, 3U> position,
    std::array<float, 3U> impulse,
    bool ignoreRotation,
    std::vector<std::string> soundPaths)
{
    EventEffect::Configure(
        definition, position, impulse, ignoreRotation);
    EventEffect::ConfigureSounds(std::move(soundPaths));
    pending_ = {};
}

DeathEffect::SpawnResult DeathEffect::OnDeath(
    bool logicAvailable, bool hasTarget,
    bool senderIsWeaponProjectile) noexcept
{
    SpawnResult result;
    if (!logicAvailable || !MakeEffect())
        return result;
    result.createEffect = true;
    result.targetChild = targetChild_ && hasTarget;
    result.ignoreSenderCar =
        effectPhysicsIgnoreSenderCar_ && senderIsWeaponProjectile;
    result.owner = this;
    result.effectId = GetMakeEffectId();
    result.definition = GetEffectDefinition();
    result.position = GetPosition();
    result.impulse = GetImpulse();
    result.ignoreRotation = GetIgnoreRotation();
    return result;
}

bool DeathEffect::GetEffectPxIgnoreSenderCar() const noexcept
{
    return effectPhysicsIgnoreSenderCar_;
}

void DeathEffect::SetEffectPxIgnoreSenderCar(bool value) noexcept
{
    effectPhysicsIgnoreSenderCar_ = value;
}

bool DeathEffect::GetTargetChild() const noexcept
{
    return targetChild_;
}

void DeathEffect::SetTargetChild(bool value) noexcept
{
    targetChild_ = value;
}

void DeathEffect::OnDeath(
    GameObject&, DamageType, GameObject* target) noexcept
{
    const auto result = OnDeath(
        logicAvailable_ || GetLogic() != nullptr,
        target != nullptr,
        senderIsWeaponProjectile_);
    if (result.createEffect)
        pending_ = result;
}

void DeathEffect::SetSpawnContext(
    bool logicAvailable,
    bool senderIsWeaponProjectile) noexcept
{
    logicAvailable_ = logicAvailable;
    senderIsWeaponProjectile_ = senderIsWeaponProjectile;
}

DeathEffect::SpawnResult
DeathEffect::ConsumeSpawnResult() noexcept
{
    const auto result = pending_;
    pending_ = {};
    return result;
}

bool DeathEffect::HasLiveEffects() const noexcept
{
    return GetEffectCount() != 0U;
}

LifeEffect::LifeEffect() noexcept : EventEffect() {}

LifeEffect::LifeEffect(Behaviors* owner) noexcept : EventEffect(owner) {}

void LifeEffect::Reset() noexcept
{
    EventEffect::Reset();
    play_ = false;
    sourceAvailable_ = false;
    soundSelectionUnit_ = 0.0F;
    playRequest_ = nullptr;
}

bool LifeEffect::OnProgress(bool sourceAvailable) noexcept
{
    if (play_ || !sourceAvailable)
        return false;
    play_ = true;
    return true;
}

bool LifeEffect::HasPlayed() const noexcept
{
    return play_;
}

void LifeEffect::OnProgress(float) noexcept
{
    if (OnProgress(sourceAvailable_))
        playRequest_ = SelectSoundPath(soundSelectionUnit_);
}

void LifeEffect::ConfigureSounds(
    std::vector<std::string> soundPaths)
{
    Reset();
    EventEffect::ConfigureSounds(std::move(soundPaths));
    sourceAvailable_ = !GetSoundPaths().empty();
    playRequest_ = nullptr;
}

void LifeEffect::SetSourceAvailable(bool value) noexcept
{
    sourceAvailable_ = value;
}

void LifeEffect::SetSoundSelectionUnit(float value) noexcept
{
    soundSelectionUnit_ = value;
}

const std::string* LifeEffect::ConsumePlayRequest() noexcept
{
    const auto* result = playRequest_;
    playRequest_ = nullptr;
    return result;
}

LowLifePoints::LowLifePoints(float lifeLevel) noexcept
{
    Reset(lifeLevel);
}

void LowLifePoints::Configure(
    const ObjectDefinition* definition,
    std::array<float, 3U> position,
    float lifeLevel) noexcept
{
    eventEffect_.Configure(definition, position);
    lifeLevel_ = lifeLevel;
    effectSeconds_ = 0.0F;
}

void LowLifePoints::Reset(float lifeLevel) noexcept
{
    lifeLevel_ = lifeLevel;
    effectSeconds_ = 0.0F;
    eventEffect_.Reset();
}

LowLifePoints::ProgressResult LowLifePoints::OnProgress(
    GameObject& gameObject, float deltaTime,
    Behavior* behavior) noexcept
{
    ProgressResult result;
    const float maximumLife = gameObject.GetMaxLife();
    const float life = gameObject.GetLife();
    const bool lowLife =
        gameObject.GetLiveState() != GameObject::LiveState::Death &&
        maximumLife > 0.0F && life > 0.0F &&
        life / maximumLife < lifeLevel_;
    if (lowLife)
    {
        if (!eventEffect_.IsEffectMaked())
            gameObject.LowLife(behavior);
        if (eventEffect_.MakeEffect())
        {
            result.activated = true;
            result.spawn = eventEffect_.GetSpawnResult(true);
        }
        effectSeconds_ += deltaTime;
    }
    else if (eventEffect_.FreeEffect())
    {
        effectSeconds_ = 0.0F;
        result.released = true;
    }
    return result;
}

float LowLifePoints::GetLifeLevel() const noexcept
{
    return lifeLevel_;
}

void LowLifePoints::SetLifeLevel(float value) noexcept
{
    lifeLevel_ = value;
}

bool LowLifePoints::IsEffectMaked() const noexcept
{
    return eventEffect_.IsEffectMaked();
}

float LowLifePoints::GetEffectSeconds() const noexcept
{
    return effectSeconds_;
}

const ObjectDefinition*
LowLifePoints::GetEffectDefinition() const noexcept
{
    return eventEffect_.GetEffectDefinition();
}

const std::array<float, 3U>&
LowLifePoints::GetEffectPosition() const noexcept
{
    return eventEffect_.GetPosition();
}

DamageEffect::DamageEffect(
    DamageType damageType, float maximumTimeLife) noexcept
    : damageType_(damageType), maximumTimeLife_(maximumTimeLife)
{
}

void DamageEffect::Configure(
    const ObjectDefinition* definition,
    DamageType damageType,
    float maximumTimeLife) noexcept
{
    damageType_ = damageType;
    maximumTimeLife_ = maximumTimeLife;
    eventEffect_.Configure(definition);
    effectSeconds_ = 0.0F;
}

void DamageEffect::ConfigureSounds(
    std::vector<std::string> soundPaths)
{
    eventEffect_.ConfigureSounds(std::move(soundPaths));
}

void DamageEffect::Reset() noexcept
{
    effectSeconds_ = 0.0F;
    playRequest_ = false;
    eventEffect_.Reset();
}

bool DamageEffect::OnDamage(DamageType damageType) noexcept
{
    if (damageType_ != damageType)
        return false;
    // DamageEffect::OnDamage always calls GiveSource3d, even while the
    // previous visual actor is still alive. The adapter consumes this
    // independently from the one-live-effect MakeEffect result.
    playRequest_ = true;
    const bool created = eventEffect_.MakeEffect();
    if (created)
        effectSeconds_ = 0.0F;
    return created;
}

void DamageEffect::OnProgress(float deltaTime) noexcept
{
    if (!eventEffect_.IsEffectMaked())
        return;
    effectSeconds_ += deltaTime;
    if (maximumTimeLife_ > 0.0F &&
        effectSeconds_ > maximumTimeLife_)
    {
        effectSeconds_ = 0.0F;
        eventEffect_.FreeEffect();
    }
}

DamageType DamageEffect::GetDamageType() const noexcept
{
    return damageType_;
}

void DamageEffect::SetDamageType(DamageType value) noexcept
{
    damageType_ = value;
}

bool DamageEffect::IsEffectMaked() const noexcept
{
    return eventEffect_.IsEffectMaked();
}

float DamageEffect::GetEffectSeconds() const noexcept
{
    return effectSeconds_;
}

EventEffect::SpawnResult DamageEffect::GetSpawnResult(
    bool created) noexcept
{
    return eventEffect_.GetSpawnResult(created);
}

const ObjectDefinition*
DamageEffect::GetEffectDefinition() const noexcept
{
    return eventEffect_.GetEffectDefinition();
}

const std::vector<std::string>&
DamageEffect::GetSoundPaths() const noexcept
{
    return eventEffect_.GetSoundPaths();
}

bool DamageEffect::HasPlayRequest() const noexcept
{
    return playRequest_;
}

const std::string* DamageEffect::ConsumePlayRequest(
    float randomUnit) noexcept
{
    if (!playRequest_)
        return nullptr;
    playRequest_ = false;
    return eventEffect_.SelectSoundPath(randomUnit);
}

void ImmortalEffect::Configure(
    const ObjectDefinition* definition,
    std::array<float, 3U> scaleK) noexcept
{
    scaleK_ = scaleK;
    eventEffect_.Configure(definition);
    Reset();
}

void ImmortalEffect::ConfigureSounds(
    std::vector<std::string> soundPaths)
{
    eventEffect_.ConfigureSounds(std::move(soundPaths));
}

void ImmortalEffect::Reset() noexcept
{
    fadeInTime_ = -1.0F;
    fadeOutTime_ = -1.0F;
    damageTime_ = -1.0F;
    effectSeconds_ = 0.0F;
    playRequest_ = false;
    eventEffect_.Reset();
}

void ImmortalEffect::OnImmortalStatus(bool status) noexcept
{
    if (status)
    {
        // EventEffect::MakeEffect keeps an existing fading actor. The source
        // deliberately does not cancel fadeOutTime_ when a new shield starts.
        eventEffect_.MakeEffect();
        // Like the Windows GiveSource3d path, sound playback belongs to the
        // activation callback and is not conditional on creating a new
        // shield actor.
        playRequest_ = true;
        fadeInTime_ = 0.0F;
    }
    else
    {
        fadeOutTime_ = 0.0F;
    }
}

void ImmortalEffect::OnDamage() noexcept
{
    if (eventEffect_.IsEffectMaked())
        damageTime_ = 0.0F;
}

void ImmortalEffect::OnProgress(float deltaTime) noexcept
{
    if (eventEffect_.IsEffectMaked() && damageTime_ >= 0.0F)
    {
        const float alpha = std::clamp(
            damageTime_ / damageSeconds, 0.0F, 1.0F);
        if (alpha >= 1.0F)
            damageTime_ = -1.0F;
        else
            damageTime_ += deltaTime;
    }
    if (eventEffect_.IsEffectMaked() && fadeInTime_ >= 0.0F)
    {
        fadeInTime_ += deltaTime;
        if (fadeInTime_ / fadeSeconds >= 1.0F)
            fadeInTime_ = -1.0F;
    }
    if (eventEffect_.IsEffectMaked() && fadeOutTime_ >= 0.0F)
    {
        fadeOutTime_ += deltaTime;
        if (fadeOutTime_ / fadeSeconds >= 1.0F)
        {
            fadeOutTime_ = -1.0F;
            damageTime_ = -1.0F;
            effectSeconds_ = 0.0F;
            eventEffect_.FreeEffect();
        }
    }
    if (eventEffect_.IsEffectMaked())
        effectSeconds_ += deltaTime;
}

bool ImmortalEffect::IsEffectMaked() const noexcept
{
    return eventEffect_.IsEffectMaked();
}

float ImmortalEffect::GetEffectSeconds() const noexcept
{
    return effectSeconds_;
}

float ImmortalEffect::GetFadeInTime() const noexcept
{
    return fadeInTime_;
}

float ImmortalEffect::GetFadeOutTime() const noexcept
{
    return fadeOutTime_;
}

float ImmortalEffect::GetDamageTime() const noexcept
{
    return damageTime_;
}

float ImmortalEffect::GetScale() const noexcept
{
    // OnProgress applies fade-in first and fade-out second, so an overlapping
    // fade-out owns the final actor scale exactly as in GameBase.cpp.
    if (fadeOutTime_ >= 0.0F)
    {
        return 1.0F - std::clamp(
            fadeOutTime_ / fadeSeconds, 0.0F, 1.0F);
    }
    if (fadeInTime_ >= 0.0F)
    {
        return std::clamp(
            fadeInTime_ / fadeSeconds, 0.0F, 1.0F);
    }
    return 1.0F;
}

float ImmortalEffect::GetDamageAlpha() const noexcept
{
    if (damageTime_ < 0.0F)
        return 1.0F;
    const float alpha = std::clamp(
        damageTime_ / damageSeconds, 0.0F, 1.0F);
    return 1.0F + 2.5F * (1.0F - alpha);
}

const ObjectDefinition*
ImmortalEffect::GetEffectDefinition() const noexcept
{
    return eventEffect_.GetEffectDefinition();
}

const std::array<float, 3U>& ImmortalEffect::GetScaleK() const noexcept
{
    return scaleK_;
}

const std::vector<std::string>&
ImmortalEffect::GetSoundPaths() const noexcept
{
    return eventEffect_.GetSoundPaths();
}

bool ImmortalEffect::HasPlayRequest() const noexcept
{
    return playRequest_;
}

const std::string* ImmortalEffect::ConsumePlayRequest(
    float randomUnit) noexcept
{
    if (!playRequest_)
        return nullptr;
    playRequest_ = false;
    return eventEffect_.SelectSoundPath(randomUnit);
}

void SlowEffect::Reset() noexcept
{
    maximumTimeLife_ = -1.0F;
    timeLife_ = 0.0F;
    weapon_ = GameObject::undefinedPlayerId;
    projectile_ = GameObject::undefinedPlayerId;
    eventEffect_.Reset();
}

bool SlowEffect::Attach(
    const ObjectDefinition* effectDefinition,
    float maximumTimeLife, std::size_t weapon,
    std::size_t projectile) noexcept
{
    // FrostRayUpdate only adds the behavior when Find<SlowEffect>() fails;
    // repeated ray contacts do not refresh the child effect's lifetime.
    if (eventEffect_.IsEffectMaked())
        return false;
    maximumTimeLife_ = maximumTimeLife;
    timeLife_ = 0.0F;
    weapon_ = weapon;
    projectile_ = projectile;
    eventEffect_.Configure(effectDefinition);
    eventEffect_.MakeEffect();
    return true;
}

bool SlowEffect::Attach(
    float maximumTimeLife, std::size_t weapon,
    std::size_t projectile) noexcept
{
    return Attach(nullptr, maximumTimeLife, weapon, projectile);
}

SlowEffect::ProgressResult SlowEffect::OnProgress(
    float deltaTime, float linearSpeed) noexcept
{
    ProgressResult result;
    if (!eventEffect_.IsEffectMaked())
        return result;
    // SlowEffect::OnProgress normalizes and clamps the actor velocity to 20
    // only while it is moving faster than both source thresholds.
    result.limitSpeed =
        linearSpeed > 1.0F && linearSpeed > maximumSpeed;
    timeLife_ += deltaTime;
    if (maximumTimeLife_ > 0.0F && timeLife_ > maximumTimeLife_)
    {
        Reset();
        result.released = true;
    }
    return result;
}

bool SlowEffect::IsEffectMaked() const noexcept
{
    return eventEffect_.IsEffectMaked();
}

float SlowEffect::GetRemainingSeconds() const noexcept
{
    if (!eventEffect_.IsEffectMaked() || maximumTimeLife_ <= 0.0F)
        return 0.0F;
    return std::max(maximumTimeLife_ - timeLife_, 0.0F);
}

std::size_t SlowEffect::GetWeapon() const noexcept
{
    return weapon_;
}

std::size_t SlowEffect::GetProjectile() const noexcept
{
    return projectile_;
}

const ObjectDefinition* SlowEffect::GetEffectDefinition() const noexcept
{
    return eventEffect_.GetEffectDefinition();
}

EventEffect::SpawnResult SlowEffect::GetSpawnResult(
    bool created) noexcept
{
    return eventEffect_.GetSpawnResult(created);
}

} // namespace r3d::game::originalrace::source
