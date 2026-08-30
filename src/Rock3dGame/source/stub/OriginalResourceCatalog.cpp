#include "OriginalResourceCatalog.h"

#include <vector>

namespace r3d::game::originalresources
{
namespace
{

#include "OriginalResourceCatalog.inc"

} // namespace

const std::vector<MeshResourceDeclaration>& originalMeshResourceCatalog()
{
    return sourceMeshResourceCatalog;
}

const std::vector<ImageResourceDeclaration>& originalImageResourceCatalog()
{
    return sourceImageResourceCatalog;
}

const std::vector<SoundResourceDeclaration>& originalSoundResourceCatalog()
{
    return sourceSoundResourceCatalog;
}

} // namespace r3d::game::originalresources
