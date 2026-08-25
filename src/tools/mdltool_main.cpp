#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mdl/mdl_reader.hpp"
#include "mdl/mdl_validator.hpp"
#include "mdl/mdl_writer.hpp"
#include "mdl/skin_family_editor.hpp"
#include "utils/cmdlib.hpp"

namespace
{
using mdl::AddSkinOptions;
using mdl::AddSkinResult;
using mdl::MdlDocument;

std::string quoted_path(const std::filesystem::path &path)
{
    return path.filename().empty() ? path_to_utf8(path) : path.filename().string();
}

std::filesystem::path default_output_path(const std::filesystem::path &input)
{
    auto output = input;
    output.replace_filename(input.stem().string() + "_new" + input.extension().string());
    return output;
}

void print_usage()
{
    std::cout
        << "Usage:\n"
        << "  mdltool++ add-skin <model.mdl> <texture.bmp> <submodel-index> <target-texture>\n"
        << "  mdltool++ inspect <model.mdl>\n"
        << "  mdltool++ validate <model.mdl>\n"
        << "  mdltool++ submodels list <model.mdl>\n"
        << "  mdltool++ textures list <model.mdl>\n"
        << "  mdltool++ skins list <model.mdl>\n";
}

MdlDocument read_model(const std::filesystem::path &path)
{
    return mdl::MdlReader::read(path);
}

void print_submodels(const MdlDocument &document)
{
    for (const auto &submodel : document.submodels)
    {
        std::cout << submodel.global_index << ": " << submodel.model_name
                  << " (bodypart: " << submodel.bodypart_name << ")\n";
    }
}

void print_textures(const MdlDocument &document)
{
    for (std::size_t index = 0; index < document.textures.size(); ++index)
    {
        const auto &texture = document.textures[index];
        std::cout << index << ": " << texture.name()
                  << " [" << texture.header.width << "x" << texture.header.height << "]\n";
    }
}

void print_skins(const MdlDocument &document)
{
    std::cout << "Skin families: " << document.skin_families.size() << "\n";
    for (std::size_t family_index = 0; family_index < document.skin_families.size(); ++family_index)
    {
        std::cout << family_index << ":";
        for (const auto texture_index : document.skin_families[family_index])
        {
            std::cout << ' ' << texture_index;
        }
        std::cout << "\n";
    }
}

void print_inspect(const MdlDocument &document)
{
    std::cout
        << "Version:           " << document.header.version << "\n"
        << "File size:         " << document.bytes.size() << "\n"
        << "Bodyparts:         " << document.header.numbodyparts << "\n"
        << "Textures:          " << document.header.numtextures << "\n"
        << "Skin families:     " << document.header.numskinfamilies << "\n";

    std::cout << "\nSubmodels:\n";
    for (const auto &submodel : document.submodels)
    {
        std::cout << "  [" << submodel.global_index << "] " << submodel.model_name
                  << " (bodypart: " << submodel.bodypart_name << ")\n";
        for (const auto &mesh : submodel.meshes)
        {
            std::cout << "    mesh " << mesh.mesh_index
                      << " skinref=" << mesh.mesh.skinref
                      << " texture[" << mesh.family0_texture_index << "]="
                      << mesh.family0_texture_name << "\n";
        }
    }

    std::cout << "\nTextures:\n";
    print_textures(document);

    std::cout << "\nSkin table:\n";
    print_skins(document);
}

AddSkinOptions parse_add_skin_options(const std::vector<std::string> &args)
{
    AddSkinOptions options;

    if (args.size() >= 4 && !(args[0].size() >= 2 && args[0][0] == '-' && args[0][1] == '-'))
    {
        options.model_path = args[0];
        options.bmp_path = args[1];
        options.submodel_index = std::stoi(args[2]);
        options.target_texture = args[3];
        options.output_path = default_output_path(options.model_path);

        for (std::size_t i = 4; i < args.size(); ++i)
        {
            const auto &arg = args[i];
            if (arg == "--all-matches")
            {
                options.all_matches = true;
            }
            else if (arg == "--in-place")
            {
                options.in_place = true;
            }
            else if (arg == "--backup")
            {
                options.backup = true;
            }
            else if (arg == "--verbose")
            {
                options.verbose = true;
            }
            else if (arg == "--force")
            {
                options.force = true;
            }
            else if (arg == "--copy-skin")
            {
                if (++i >= args.size())
                {
                    throw std::runtime_error("Error: --copy-skin requires a value.");
                }
                options.copy_skin_index = std::stoi(args[i]);
            }
            else if (arg == "--output")
            {
                if (++i >= args.size())
                {
                    throw std::runtime_error("Error: --output requires a value.");
                }
                options.output_path = args[i];
            }
            else
            {
                throw std::runtime_error("Error: unknown add-skin option \"" + arg + "\".");
            }
        }
        return options;
    }

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const auto &arg = args[i];
        auto require_value = [&](const char *label) -> const std::string &
        {
            if (++i >= args.size())
            {
                throw std::runtime_error(std::string("Error: ") + label + " requires a value.");
            }
            return args[i];
        };

        if (arg == "--model")
        {
            options.model_path = require_value("--model");
        }
        else if (arg == "--bmp")
        {
            options.bmp_path = require_value("--bmp");
        }
        else if (arg == "--submodel")
        {
            options.submodel_index = std::stoi(require_value("--submodel"));
        }
        else if (arg == "--target")
        {
            options.target_texture = require_value("--target");
        }
        else if (arg == "--copy-skin")
        {
            options.copy_skin_index = std::stoi(require_value("--copy-skin"));
        }
        else if (arg == "--output")
        {
            options.output_path = require_value("--output");
        }
        else if (arg == "--all-matches")
        {
            options.all_matches = true;
        }
        else if (arg == "--in-place")
        {
            options.in_place = true;
        }
        else if (arg == "--backup")
        {
            options.backup = true;
        }
        else if (arg == "--verbose")
        {
            options.verbose = true;
        }
        else if (arg == "--force")
        {
            options.force = true;
        }
        else
        {
            throw std::runtime_error("Error: unknown add-skin option \"" + arg + "\".");
        }
    }

    if (options.model_path.empty() || options.bmp_path.empty() || options.target_texture.empty())
    {
        throw std::runtime_error("Error: add-skin requires model, bmp, submodel, and target texture.");
    }
    if (options.output_path.empty() && !options.in_place)
    {
        options.output_path = default_output_path(options.model_path);
    }
    return options;
}

void safe_replace_in_place(const std::filesystem::path &model_path,
                           const std::filesystem::path &temp_path,
                           bool backup)
{
    const auto backup_path = model_path.string() + ".bak";
    if (backup)
    {
        std::filesystem::copy_file(
            model_path, backup_path, std::filesystem::copy_options::overwrite_existing);
    }
    else
    {
        std::filesystem::remove(backup_path);
    }

    std::filesystem::remove(model_path);
    std::filesystem::rename(temp_path, model_path);
}

AddSkinResult execute_add_skin(const AddSkinOptions &options)
{
    auto document = read_model(options.model_path);
    auto mutable_options = options;
    if (!mutable_options.in_place && mutable_options.output_path.empty())
    {
        mutable_options.output_path = default_output_path(mutable_options.model_path);
    }

    auto result = mdl::SkinFamilyEditor::add_skin(document, mutable_options);

    if (mutable_options.in_place)
    {
        auto temp_path = mutable_options.model_path;
        temp_path += ".tmp";
        mdl::MdlWriter::write(document, temp_path);
        mdl::MdlValidator::validate_file(temp_path);
        safe_replace_in_place(mutable_options.model_path, temp_path, mutable_options.backup);
        result.output_path = mutable_options.model_path;
    }
    else
    {
        mdl::MdlWriter::write(document, mutable_options.output_path);
        mdl::MdlValidator::validate_file(mutable_options.output_path);
        result.output_path = mutable_options.output_path;
    }

    return result;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        const auto native_args = get_native_args(argc, argv);
        std::vector<std::string> args;
        args.reserve(native_args.size());
        for (const auto &arg : native_args)
        {
            args.push_back(path_to_utf8(arg));
        }

        if (args.size() < 2)
        {
            print_usage();
            return 1;
        }

        if (args[1] == "inspect")
        {
            if (args.size() != 3)
            {
                throw std::runtime_error("Error: inspect requires <model.mdl>.");
            }
            print_inspect(read_model(args[2]));
            return 0;
        }

        if (args[1] == "validate")
        {
            if (args.size() != 3)
            {
                throw std::runtime_error("Error: validate requires <model.mdl>.");
            }
            mdl::MdlValidator::validate_file(args[2]);
            std::cout << "Valid MDL: " << quoted_path(args[2]) << "\n";
            return 0;
        }

        if (args[1] == "submodels" && args.size() == 4 && args[2] == "list")
        {
            print_submodels(read_model(args[3]));
            return 0;
        }

        if (args[1] == "textures" && args.size() == 4 && args[2] == "list")
        {
            print_textures(read_model(args[3]));
            return 0;
        }

        if (args[1] == "skins" && args.size() == 4 && args[2] == "list")
        {
            print_skins(read_model(args[3]));
            return 0;
        }

        if (args[1] == "add-skin")
        {
            const std::vector<std::string> command_args(args.begin() + 2, args.end());
            auto options = parse_add_skin_options(command_args);
            auto result = execute_add_skin(options);

            std::cout
                << "Input model:       " << quoted_path(options.model_path) << "\n"
                << "Submodel:          " << options.submodel_index << "\n"
                << "Target texture:    " << options.target_texture << "\n"
                << "Imported texture:  " << quoted_path(options.bmp_path) << "\n"
                << "New skin index:    " << result.new_skin_index << "\n"
                << "Output model:      " << quoted_path(result.output_path) << "\n"
                << "Reference:         " << quoted_path(result.output_path) << ":"
                << options.submodel_index << ":" << result.new_skin_index << "\n";
            return 0;
        }

        print_usage();
        return 1;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << "\n";
        return 1;
    }
}
