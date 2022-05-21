#include "audio_controller.h"

#include <scene/node.h>
#include <math/math_util.h>

namespace hyperion::v2 {

AudioController::AudioController()
    : AudioController(nullptr)
{
}

AudioController::AudioController(std::unique_ptr<AudioSource> &&source)
    : PlaybackController("AudioController"),
      m_source(std::move(source)),
      m_timer(0.0f)
{
}

void AudioController::SetSource(std::unique_ptr<AudioSource> &&source)
{
    Stop();

    m_source = std::move(source);
}

void AudioController::Play(float speed, LoopMode loop_mode)
{
    if (m_source == nullptr) {
        return;
    }

    m_source->Play();
    m_source->SetPitch(speed);
    m_source->SetLoop(loop_mode == LoopMode::REPEAT);

    PlaybackController::Play(speed, loop_mode);
}

void AudioController::Stop()
{
    if (m_source != nullptr) {
        m_source->Stop();
    }

    PlaybackController::Stop();
}

void AudioController::OnAdded()
{
    m_last_position = GetParent()->GetWorldTranslation();
}

void AudioController::OnRemoved()
{
    if (m_source != nullptr) {
        m_source->Stop();
    }
}

void AudioController::OnUpdate(GameCounter::TickUnit delta)
{
    if (m_source != nullptr && IsPlaying()) {
        const auto new_position = GetParent()->GetWorldTranslation();

        if (m_state.loop_mode == LoopMode::ONCE) {
            switch (m_source->GetState()) {
            case AudioSource::State::PLAYING:
                break;
            case AudioSource::State::PAUSED:
                SetPlaybackState(PlaybackState::PAUSED); break;
            case AudioSource::State::STOPPED: // fallthrough
            case AudioSource::State::UNDEFINED:
                SetPlaybackState(PlaybackState::STOPPED); break;
            }
        }

        if (!MathUtil::Approximately(new_position, m_last_position)) {
            const auto position_change = new_position - m_last_position;
            const auto time_change = (m_timer + delta) - m_timer;
            const auto velocity = position_change / time_change;

            m_source->SetPosition(new_position);
            m_source->SetVelocity(velocity);

            m_last_position = new_position;
        }
    }
    
    m_timer += delta;

}

} // namespace hyperion::v2
