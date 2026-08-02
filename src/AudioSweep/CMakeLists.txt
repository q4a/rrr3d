# The engine rev sound, without the game.
#
# One source voice shaped exactly like snd::Proxy's, with SetFrequencyRatio
# swept from 0 to 1 the way SoundMotor::OnMotor sweeps it. That sweep is what
# provoked the out-of-bounds write in FAudio's resampler, and reproducing it
# here takes under a second instead of a race and eight repetitions.
#
# Nothing of the game or the graphics stack is involved, so it runs under the
# asan preset cheaply -- which is the point, since extern/faudio is built from
# source precisely so the sanitizer can see inside it.
project(AudioSweep)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE XPlatform)
