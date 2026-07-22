#pragma once

#include <string>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::audio
{

class SdlAudioBackend;

bool runSdlAudioSmokeTest(SdlAudioBackend &audio, const r3d::resource::ResourceFileSystem &resources,
                          std::string &error);

} // namespace rrr3d::audio
