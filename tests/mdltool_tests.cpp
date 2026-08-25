#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "format/mdl.hpp"
#include "gui/editor_state.hpp"
#include "mdl/mdl_reader.hpp"
#include "mdl/mdl_validator.hpp"
#include "mdl/mdl_writer.hpp"
#include "mdl/skin_family_editor.hpp"
#include "mdl/texture_importer.hpp"

namespace
{
constexpr std::size_t kAlign = 4;

std::size_t align_up(std::size_t value)
{
    return (value + (kAlign - 1)) & ~(kAlign - 1);
}

void expect(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

template <typename T>
void append_struct(std::vector<std::byte> &buffer, const T &value)
{
    const auto *raw = reinterpret_cast<const std::byte *>(&value);
    buffer.insert(buffer.end(), raw, raw + sizeof(T));
}

void append_bytes(std::vector<std::byte> &buffer, const std::vector<std::byte> &bytes)
{
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

void align_buffer(std::vector<std::byte> &buffer)
{
    buffer.resize(align_up(buffer.size()), std::byte{0});
}

std::vector<std::byte> make_texture_blob(std::uint8_t pixel, std::uint8_t palette_seed)
{
    std::vector<std::byte> bytes(1 + 256 * 3);
    bytes[0] = static_cast<std::byte>(pixel);
    for (int i = 0; i < 256 * 3; ++i)
    {
        bytes[1 + i] = static_cast<std::byte>((palette_seed + i) & 0xFF);
    }
    return bytes;
}

std::filesystem::path temp_root()
{
    const auto root = std::filesystem::temp_directory_path() / "mdltool_tests";
    std::filesystem::create_directories(root);
    return root;
}

void write_file(const std::filesystem::path &path, const std::vector<std::byte> &bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
    {
        throw std::runtime_error("Failed to write test fixture.");
    }
}

void write_u16(std::vector<std::byte> &buffer, std::uint16_t value)
{
    buffer.push_back(static_cast<std::byte>(value & 0xFF));
    buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
}

void write_u32(std::vector<std::byte> &buffer, std::uint32_t value)
{
    buffer.push_back(static_cast<std::byte>(value & 0xFF));
    buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
}

std::filesystem::path write_bmp8(const std::filesystem::path &path, std::uint8_t pixel)
{
    std::vector<std::byte> bytes;
    const std::uint32_t off_bits = 14 + 40 + 256 * 4;
    const std::uint32_t file_size = off_bits + 4;

    write_u16(bytes, 0x4D42);
    write_u32(bytes, file_size);
    write_u16(bytes, 0);
    write_u16(bytes, 0);
    write_u32(bytes, off_bits);

    write_u32(bytes, 40);
    write_u32(bytes, 1);
    write_u32(bytes, 1);
    write_u16(bytes, 1);
    write_u16(bytes, 8);
    write_u32(bytes, 0);
    write_u32(bytes, 4);
    write_u32(bytes, 0);
    write_u32(bytes, 0);
    write_u32(bytes, 256);
    write_u32(bytes, 256);

    for (int i = 0; i < 256; ++i)
    {
        bytes.push_back(static_cast<std::byte>(i));
        bytes.push_back(static_cast<std::byte>(i));
        bytes.push_back(static_cast<std::byte>(i));
        bytes.push_back(std::byte{0});
    }

    bytes.push_back(static_cast<std::byte>(pixel));
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});

    write_file(path, bytes);
    return path;
}

std::filesystem::path write_bmp24(const std::filesystem::path &path)
{
    std::vector<std::byte> bytes;
    const std::uint32_t off_bits = 14 + 40;
    const std::uint32_t file_size = off_bits + 4;

    write_u16(bytes, 0x4D42);
    write_u32(bytes, file_size);
    write_u16(bytes, 0);
    write_u16(bytes, 0);
    write_u32(bytes, off_bits);

    write_u32(bytes, 40);
    write_u32(bytes, 1);
    write_u32(bytes, 1);
    write_u16(bytes, 1);
    write_u16(bytes, 24);
    write_u32(bytes, 0);
    write_u32(bytes, 4);
    write_u32(bytes, 0);
    write_u32(bytes, 0);
    write_u32(bytes, 0);
    write_u32(bytes, 0);

    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});

    write_file(path, bytes);
    return path;
}

std::filesystem::path create_sample_mdl(const std::filesystem::path &path)
{
    StudioHeader header{};
    header.ident = IDSTUDIOHEADER;
    header.version = STUDIO_VERSION;
    std::strcpy(header.name, "banners.mdl");
    header.numbodyparts = 1;
    header.numtextures = 3;
    header.numskinref = 3;
    header.numskinfamilies = 1;

    std::vector<std::byte> buffer(sizeof(StudioHeader), std::byte{0});

    header.bodypartindex = static_cast<int>(buffer.size());
    StudioBodyPart bodypart{};
    std::strcpy(bodypart.name, "body");
    bodypart.nummodels = 3;
    append_struct(buffer, bodypart);
    align_buffer(buffer);

    bodypart.modelindex = static_cast<int>(buffer.size());
    std::memcpy(buffer.data() + header.bodypartindex, &bodypart, sizeof(bodypart));

    std::array<StudioModel, 3> models{};
    models[0].nummesh = 1;
    models[1].nummesh = 1;
    models[2].nummesh = 2;
    std::strcpy(models[0].name, "sub0");
    std::strcpy(models[1].name, "sub1");
    std::strcpy(models[2].name, "sub2");

    const auto model_table_offset = buffer.size();
    const auto mesh_table_offset = model_table_offset + sizeof(models);
    models[0].meshindex = static_cast<int>(mesh_table_offset);
    models[1].meshindex = static_cast<int>(mesh_table_offset + sizeof(StudioMesh));
    models[2].meshindex = static_cast<int>(mesh_table_offset + sizeof(StudioMesh) * 2);

    for (const auto &model : models)
    {
        append_struct(buffer, model);
    }

    StudioMesh mesh0{};
    mesh0.skinref = 0;
    StudioMesh mesh1{};
    mesh1.skinref = 2;
    StudioMesh mesh2a{};
    mesh2a.skinref = 1;
    StudioMesh mesh2b{};
    mesh2b.skinref = 2;
    append_struct(buffer, mesh0);
    append_struct(buffer, mesh1);
    append_struct(buffer, mesh2a);
    append_struct(buffer, mesh2b);
    align_buffer(buffer);

    header.textureindex = static_cast<int>(buffer.size());
    std::array<StudioTexture, 3> textures{};
    std::strcpy(textures[0].name, "base.bmp");
    std::strcpy(textures[1].name, "def_post.bmp");
    std::strcpy(textures[2].name, "detail.bmp");
    for (auto &texture : textures)
    {
        texture.width = 1;
        texture.height = 1;
    }
    for (const auto &texture : textures)
    {
        append_struct(buffer, texture);
    }
    align_buffer(buffer);

    header.skinindex = static_cast<int>(buffer.size());
    std::array<std::int16_t, 3> family0{0, 1, 2};
    append_bytes(buffer, std::vector<std::byte>(
                             reinterpret_cast<const std::byte *>(family0.data()),
                             reinterpret_cast<const std::byte *>(family0.data()) + sizeof(family0)));
    align_buffer(buffer);

    header.texturedataindex = static_cast<int>(buffer.size());
    const auto tex0 = make_texture_blob(0x01, 0x10);
    const auto tex1 = make_texture_blob(0x02, 0x20);
    const auto tex2 = make_texture_blob(0x03, 0x30);

    textures[0].index = static_cast<int>(buffer.size());
    append_bytes(buffer, tex0);
    textures[1].index = static_cast<int>(buffer.size());
    append_bytes(buffer, tex1);
    textures[2].index = static_cast<int>(buffer.size());
    append_bytes(buffer, tex2);
    align_buffer(buffer);

    std::memcpy(buffer.data() + header.textureindex, textures.data(), sizeof(textures));

    header.length = static_cast<int>(buffer.size());
    std::memcpy(buffer.data(), &header, sizeof(header));

    write_file(path, buffer);
    return path;
}

void expect_throw(const std::function<void()> &fn, const std::string &contains)
{
    try
    {
        fn();
    }
    catch (const std::exception &exception)
    {
        expect(std::string(exception.what()).find(contains) != std::string::npos,
               "Unexpected error: " + std::string(exception.what()));
        return;
    }
    throw std::runtime_error("Expected exception: " + contains);
}

void test_read_valid_mdl()
{
    const auto path = create_sample_mdl(temp_root() / "valid.mdl");
    const auto document = mdl::MdlReader::read(path);
    expect(document.header.version == 10, "Expected MDL v10.");
    expect(document.submodels.size() == 3, "Expected 3 submodels.");
    expect(document.textures.size() == 3, "Expected 3 textures.");
    expect(document.skin_families.size() == 1, "Expected 1 skin family.");
}

void test_invalid_signature()
{
    const auto path = create_sample_mdl(temp_root() / "bad_sig.mdl");
    auto document = mdl::MdlReader::read(path);
    document.bytes[0] = std::byte{'B'};
    write_file(path, document.bytes);
    expect_throw([&]() { mdl::MdlReader::read(path); }, "signature");
}

void test_invalid_version()
{
    const auto path = create_sample_mdl(temp_root() / "bad_ver.mdl");
    auto document = mdl::MdlReader::read(path);
    auto header = document.header;
    header.version = 11;
    std::memcpy(document.bytes.data(), &header, sizeof(header));
    write_file(path, document.bytes);
    expect_throw([&]() { mdl::MdlReader::read(path); }, "version");
}

void test_invalid_offsets()
{
    const auto path = create_sample_mdl(temp_root() / "bad_offset.mdl");
    auto document = mdl::MdlReader::read(path);
    auto header = document.header;
    header.textureindex = header.length + 64;
    std::memcpy(document.bytes.data(), &header, sizeof(header));
    write_file(path, document.bytes);
    bool failed = false;
    try
    {
        static_cast<void>(mdl::MdlReader::read(path));
    }
    catch (const std::exception &)
    {
        failed = true;
    }
    expect(failed, "Expected invalid offset fixture to fail.");
}

void test_submodel_list()
{
    const auto document = mdl::MdlReader::read(create_sample_mdl(temp_root() / "submodels.mdl"));
    expect(document.submodels[2].global_index == 2, "Expected global submodel index 2.");
}

void test_texture_list()
{
    const auto document = mdl::MdlReader::read(create_sample_mdl(temp_root() / "textures.mdl"));
    expect(document.textures[1].name() == "def_post.bmp", "Expected def_post.bmp.");
}

void test_skin_table()
{
    const auto document = mdl::MdlReader::read(create_sample_mdl(temp_root() / "skins.mdl"));
    expect(document.skin_families[0][1] == 1, "Expected skin family[0][1] == 1.");
}

void test_add_new_texture_and_skin_family()
{
    const auto model_path = create_sample_mdl(temp_root() / "add_skin.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "ban1.bmp", 0x55);
    auto document = mdl::MdlReader::read(model_path);

    mdl::AddSkinOptions options;
    options.model_path = model_path;
    options.bmp_path = bmp_path;
    options.output_path = temp_root() / "add_skin_out.mdl";
    options.submodel_index = 2;
    options.target_texture = "def_post.bmp";

    const auto result = mdl::SkinFamilyEditor::add_skin(document, options);
    expect(result.new_skin_index == 1, "Expected new skin family index 1.");
    expect(document.textures.size() == 4, "Expected added texture.");
    expect(document.skin_families.size() == 2, "Expected copied skin family.");
    expect(document.skin_families[1][1] == 3, "Expected replaced skinref to point to new texture.");
    expect(document.skin_families[1][0] == 0, "Expected untouched skinref 0.");
    expect(document.skin_families[1][2] == 2, "Expected untouched skinref 2.");
}

void test_rewrite_and_reopen()
{
    std::cout << "  step: create fixture\n";
    const auto model_path = create_sample_mdl(temp_root() / "rewrite.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "rewrite.bmp", 0x22);
    std::cout << "  step: read original\n";
    auto document = mdl::MdlReader::read(model_path);
    const auto original_size = std::filesystem::file_size(model_path);

    mdl::AddSkinOptions options;
    options.model_path = model_path;
    options.bmp_path = bmp_path;
    options.output_path = temp_root() / "rewrite_out.mdl";
    options.submodel_index = 2;
    options.target_texture = "def_post.bmp";

    std::cout << "  step: add skin\n";
    mdl::SkinFamilyEditor::add_skin(document, options);
    std::cout << "  step: write file\n";
    mdl::MdlWriter::write(document, options.output_path);
    std::cout << "  step: reopen\n";
    const auto reopened = mdl::MdlReader::read(options.output_path);
    std::cout << "  step: file_size\n";
    const auto new_size = std::filesystem::file_size(options.output_path);
    const auto growth = new_size - original_size;
    const std::uintmax_t texture_payload_growth = static_cast<std::uintmax_t>(1 + 256 * 3);
    const std::uintmax_t expected_max_growth =
        texture_payload_growth + sizeof(StudioTexture) + sizeof(std::int16_t) * 3 + 128;
    expect(reopened.textures.size() == 4, "Expected rewritten model to contain new texture.");
    expect(reopened.skin_families.size() == 2, "Expected rewritten model to contain new skin family.");
    expect(growth >= texture_payload_growth, "Expected file size growth to include one new texture payload.");
    expect(growth <= expected_max_growth, "Expected file size growth to stay near one rebuilt texture payload.");
    expect(growth < original_size / 2, "Expected writer not to duplicate most of the original MDL.");
}

void test_preserve_sections()
{
    const auto model_path = create_sample_mdl(temp_root() / "preserve.mdl");
    const auto original = mdl::MdlReader::read(model_path);
    const auto bmp_path = write_bmp8(temp_root() / "preserve.bmp", 0x44);
    auto document = mdl::MdlReader::read(model_path);

    mdl::AddSkinOptions options;
    options.model_path = model_path;
    options.bmp_path = bmp_path;
    options.output_path = temp_root() / "preserve_out.mdl";
    options.submodel_index = 2;
    options.target_texture = "def_post.bmp";

    mdl::SkinFamilyEditor::add_skin(document, options);
    mdl::MdlWriter::write(document, options.output_path);
    const auto rewritten = mdl::MdlReader::read(options.output_path);

    expect(original.bodyparts[0].modelindex == rewritten.bodyparts[0].modelindex,
           "Expected bodypart section to stay unchanged.");
    expect(original.models[2].meshindex == rewritten.models[2].meshindex,
           "Expected model mesh offset to stay unchanged.");
}

void test_reject_24bit_bmp()
{
    const auto path = write_bmp24(temp_root() / "bad24.bmp");
    expect_throw([&]() { mdl::TextureImporter::import_bmp(path); }, "indexed 8-bit BMP");
}

void test_bad_submodel_index()
{
    const auto model_path = create_sample_mdl(temp_root() / "bad_submodel.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "bad_submodel.bmp", 0x66);
    auto document = mdl::MdlReader::read(model_path);

    mdl::AddSkinOptions options;
    options.model_path = model_path;
    options.bmp_path = bmp_path;
    options.output_path = temp_root() / "bad_submodel_out.mdl";
    options.submodel_index = 7;
    options.target_texture = "def_post.bmp";

    expect_throw([&]() { mdl::SkinFamilyEditor::add_skin(document, options); }, "out of range");
}

void test_missing_target_texture()
{
    const auto model_path = create_sample_mdl(temp_root() / "missing_target.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "missing_target.bmp", 0x77);
    auto document = mdl::MdlReader::read(model_path);

    mdl::AddSkinOptions options;
    options.model_path = model_path;
    options.bmp_path = bmp_path;
    options.output_path = temp_root() / "missing_target_out.mdl";
    options.submodel_index = 2;
    options.target_texture = "missing.bmp";

    expect_throw([&]() { mdl::SkinFamilyEditor::add_skin(document, options); }, "is not used by submodel 2");
}

void test_multiple_matches()
{
    const auto model_path = create_sample_mdl(temp_root() / "multiple_matches.mdl");
    auto document = mdl::MdlReader::read(model_path);
    document.submodels[2].meshes[1].family0_texture_name = "def_post.bmp";
    const auto bmp_path = write_bmp8(temp_root() / "multiple_matches.bmp", 0x88);

    mdl::AddSkinOptions options;
    options.model_path = model_path;
    options.bmp_path = bmp_path;
    options.output_path = temp_root() / "multiple_matches_out.mdl";
    options.submodel_index = 2;
    options.target_texture = "def_post.bmp";

    expect_throw([&]() { mdl::SkinFamilyEditor::add_skin(document, options); }, "matches multiple meshes");
}

void test_in_place_flow()
{
    const auto model_path = create_sample_mdl(temp_root() / "inplace.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "inplace.bmp", 0x99);
    auto document = mdl::MdlReader::read(model_path);

    mdl::AddSkinOptions options;
    options.model_path = model_path;
    options.bmp_path = bmp_path;
    options.output_path = model_path;
    options.submodel_index = 2;
    options.target_texture = "def_post.bmp";

    mdl::SkinFamilyEditor::add_skin(document, options);
    mdl::MdlWriter::write(document, model_path);
    const auto reopened = mdl::MdlReader::read(model_path);
    expect(reopened.skin_families.size() == 2, "Expected in-place rewrite to keep new skin family.");
}

void test_editor_state_add_skin_and_undo()
{
    gui::EditorState state;
    const auto model_path = create_sample_mdl(temp_root() / "editor_add.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "editor_add.bmp", 0xAB);

    expect(state.open_model(model_path), "Expected EditorState to open a valid model.");

    gui::AddSkinRequest request;
    request.submodel_index = 2;
    request.mesh_index = 0;
    request.copy_skin_family = 0;
    request.bmp_path = bmp_path;

    expect(state.add_skin(request), "Expected EditorState add_skin to succeed.");
    expect(state.document()->skin_families.size() == 2, "Expected added skin family in EditorState.");
    expect(state.document()->textures.size() == 4, "Expected added texture in EditorState.");
    expect(state.undo_last_operation(), "Expected undo of add_skin to succeed.");
    expect(state.document()->skin_families.size() == 1, "Expected skin families restored after undo.");
    expect(state.document()->textures.size() == 3, "Expected textures restored after undo.");
}

void test_editor_state_remove_skin_and_undo()
{
    gui::EditorState state;
    const auto model_path = create_sample_mdl(temp_root() / "editor_remove.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "editor_remove.bmp", 0xBC);

    expect(state.open_model(model_path), "Expected EditorState to open a valid model.");
    gui::AddSkinRequest request;
    request.submodel_index = 2;
    request.mesh_index = 0;
    request.copy_skin_family = 0;
    request.bmp_path = bmp_path;
    expect(state.add_skin(request), "Expected EditorState add_skin to succeed before remove.");

    expect(state.remove_skin_family(1), "Expected removing the added skin family to succeed.");
    expect(state.document()->skin_families.size() == 1, "Expected skin family removal to shrink the table.");
    expect(state.undo_last_operation(), "Expected undo of remove_skin to succeed.");
    expect(state.document()->skin_families.size() == 2, "Expected removed skin family to be restored.");
}

void test_editor_state_replace_texture_and_undo()
{
    gui::EditorState state;
    const auto model_path = create_sample_mdl(temp_root() / "editor_replace.mdl");
    const auto bmp_path = write_bmp8(temp_root() / "editor_replace.bmp", 0xCD);

    expect(state.open_model(model_path), "Expected EditorState to open a valid model.");

    gui::ReplaceTextureRequest request;
    request.texture_index = 1;
    request.bmp_path = bmp_path;
    request.keep_name = true;
    const auto previous_name = state.document()->textures[1].name();
    const auto previous_data = state.document()->textures[1].data;

    expect(state.replace_texture(request), "Expected texture replacement to succeed.");
    expect(state.document()->textures[1].name() == previous_name, "Expected keep_name to preserve texture name.");
    expect(state.document()->textures[1].data != previous_data, "Expected texture payload to change.");
    expect(state.undo_last_operation(), "Expected undo of replace_texture to succeed.");
    expect(state.document()->textures[1].data == previous_data, "Expected texture payload restored after undo.");
}

} // namespace

int main()
{
    std::cout.setf(std::ios::unitbuf);
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"read valid mdl", test_read_valid_mdl},
        {"invalid signature", test_invalid_signature},
        {"invalid version", test_invalid_version},
        {"invalid offsets", test_invalid_offsets},
        {"submodel list", test_submodel_list},
        {"texture list", test_texture_list},
        {"skin table", test_skin_table},
        {"add new texture and skin family", test_add_new_texture_and_skin_family},
        {"rewrite and reopen", test_rewrite_and_reopen},
        {"preserve sections", test_preserve_sections},
        {"reject 24bit bmp", test_reject_24bit_bmp},
        {"bad submodel index", test_bad_submodel_index},
        {"missing target texture", test_missing_target_texture},
        {"multiple matches", test_multiple_matches},
        {"in-place flow", test_in_place_flow},
        {"editor state add skin and undo", test_editor_state_add_skin_and_undo},
        {"editor state remove skin and undo", test_editor_state_remove_skin_and_undo},
        {"editor state replace texture and undo", test_editor_state_replace_texture_and_undo},
    };

    for (const auto &[name, fn] : tests)
    {
        std::cout << "[RUN] " << name << "\n";
        fn();
        std::cout << "[PASS] " << name << "\n";
    }
    return 0;
}
