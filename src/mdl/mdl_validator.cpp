#include "mdl/mdl_validator.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "mdl/mdl_reader.hpp"

namespace mdl
{
namespace
{
template <typename T>
void ensure_range(const MdlDocument &document, int offset, int count, const char *label)
{
    if (offset < 0 || count < 0)
    {
        throw std::runtime_error(std::string("Error: invalid ") + label + " range.");
    }

    const std::size_t total = static_cast<std::size_t>(count) * sizeof(T);
    if (static_cast<std::size_t>(offset) + total > document.bytes.size())
    {
        throw std::runtime_error(std::string("Error: ") + label + " is out of bounds.");
    }
}

void ensure_bytes(const MdlDocument &document, int offset, std::size_t size, const char *label)
{
    if (offset < 0 || static_cast<std::size_t>(offset) + size > document.bytes.size())
    {
        throw std::runtime_error(std::string("Error: ") + label + " is out of bounds.");
    }
}
} // namespace

void MdlValidator::validate(const MdlDocument &document)
{
    if (document.header.ident != IDSTUDIOHEADER)
    {
        throw std::runtime_error("Error: unsupported MDL signature. Expected IDST.");
    }
    if (document.header.version != STUDIO_VERSION)
    {
        throw std::runtime_error(
            "Error: unsupported MDL version " + std::to_string(document.header.version) + ".");
    }
    if (document.header.length != static_cast<int>(document.bytes.size()))
    {
        throw std::runtime_error(
            "Error: file size does not match StudioHeader::length.");
    }

    ensure_range<StudioBone>(document, document.header.boneindex, document.header.numbones, "bone table");
    ensure_range<StudioBoneController>(
        document, document.header.bonecontrollerindex, document.header.numbonecontrollers, "bone controller table");
    ensure_range<StudioHitbox>(document, document.header.hitboxindex, document.header.numhitboxes, "hitbox table");
    ensure_range<StudioSequenceDescription>(document, document.header.seqindex, document.header.numseq, "sequence table");
    ensure_range<StudioSequenceGroup>(
        document, document.header.seqgroupindex, document.header.numseqgroups, "sequence group table");
    ensure_range<StudioTexture>(document, document.header.textureindex, document.header.numtextures, "texture table");
    ensure_range<std::int16_t>(
        document,
        document.header.skinindex,
        document.header.numskinref * document.header.numskinfamilies,
        "skin family table");
    ensure_range<StudioBodyPart>(
        document, document.header.bodypartindex, document.header.numbodyparts, "bodypart table");
    ensure_range<StudioAttachment>(
        document, document.header.attachmentindex, document.header.numattachments, "attachment table");
    ensure_bytes(
        document,
        document.header.transitionindex,
        static_cast<std::size_t>(document.header.numtransitions) *
            static_cast<std::size_t>(document.header.numtransitions),
        "transition graph");

    for (int sequence_index = 0; sequence_index < document.header.numseq; ++sequence_index)
    {
        StudioSequenceDescription sequence{};
        std::memcpy(&sequence,
                    document.bytes.data() + document.header.seqindex +
                        static_cast<std::size_t>(sequence_index) * sizeof(StudioSequenceDescription),
                    sizeof(sequence));
        ensure_range<StudioAnimationEvent>(
            document, sequence.eventindex, sequence.numevents, "sequence events");
        if (sequence.numblends < 0)
        {
            throw std::runtime_error("Error: negative sequence blend count.");
        }
        ensure_bytes(
            document,
            sequence.animindex,
            static_cast<std::size_t>(sequence.numblends) *
                static_cast<std::size_t>(document.header.numbones) *
                sizeof(StudioAnimationFrameOffset),
            "sequence animation frame table");
    }

    int expected_model_count = 0;
    for (const auto &bodypart : document.bodyparts)
    {
        ensure_range<StudioModel>(document, bodypart.modelindex, bodypart.nummodels, "model table");
        expected_model_count += bodypart.nummodels;
    }
    if (expected_model_count != static_cast<int>(document.models.size()))
    {
        throw std::runtime_error("Error: parsed model count does not match bodypart table.");
    }

    for (std::size_t model_index = 0; model_index < document.models.size(); ++model_index)
    {
        const auto &model = document.models[model_index];
        ensure_range<StudioMesh>(document, model.meshindex, model.nummesh, "mesh table");
        ensure_bytes(
            document,
            model.vertinfoindex,
            static_cast<std::size_t>(std::max(model.numverts, 0)),
            "vertex bone table");
        ensure_bytes(
            document,
            model.norminfoindex,
            static_cast<std::size_t>(std::max(model.numnorms, 0)),
            "normal bone table");
        ensure_bytes(
            document,
            model.vertindex,
            static_cast<std::size_t>(std::max(model.numverts, 0)) * sizeof(Vector3),
            "vertex table");
        ensure_bytes(
            document,
            model.normindex,
            static_cast<std::size_t>(std::max(model.numnorms, 0)) * sizeof(Vector3),
            "normal table");

        for (const auto &mesh : document.meshes[model_index])
        {
            if (mesh.numtris > 0)
            {
                ensure_bytes(document, mesh.triindex, 1, "triangle command stream");
            }
            if (mesh.skinref < 0 || mesh.skinref >= document.header.numskinref)
            {
                throw std::runtime_error("Error: mesh skinref is out of range.");
            }
        }
    }

    for (const auto &family : document.skin_families)
    {
        if (static_cast<int>(family.size()) != document.header.numskinref)
        {
            throw std::runtime_error("Error: skin family size does not match numskinref.");
        }
        for (const auto texture_index : family)
        {
            if (texture_index < 0 || texture_index >= static_cast<int>(document.textures.size()))
            {
                throw std::runtime_error("Error: skin family references an invalid texture index.");
            }
        }
    }

    for (const auto &texture : document.textures)
    {
        if (texture.header.width <= 0 || texture.header.height <= 0)
        {
            throw std::runtime_error("Error: texture dimensions must be positive.");
        }
        if (static_cast<int>(texture.data.size()) != texture.expected_data_size())
        {
            throw std::runtime_error("Error: texture payload size is invalid.");
        }
    }
}

void MdlValidator::validate_file(const std::filesystem::path &path)
{
    const auto document = MdlReader::read(path);
    validate(document);
}
} // namespace mdl
