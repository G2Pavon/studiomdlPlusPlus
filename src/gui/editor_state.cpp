#include "editor_state.hpp"

#include <sstream>

#include "mdl/mdl_reader.hpp"
#include "mdl/mdl_validator.hpp"
#include "mdl/mdl_writer.hpp"
#include "utils/cmdlib.hpp"

namespace gui
{
namespace
{
std::string filename_or_path(const std::filesystem::path &path)
{
    return path.filename().empty() ? path_to_utf8(path) : path.filename().string();
}
} // namespace

EditorState::EditorState() = default;

bool EditorState::open_model(const std::filesystem::path &path)
{
    try
    {
        auto document = std::make_unique<mdl::MdlDocument>(mdl::MdlReader::read(path));
        document_ = std::move(document);
        original_path_ = path;
        current_path_.clear();
        selection_ = {};
        if (!document_->submodels.empty())
        {
            selection_.submodel_index = 0;
        }
        valid_ = true;
        unsaved_changes_ = false;
        reset_undo();
        push_log(LogLevel::Success, "Opened model: " + path_to_utf8(path));
        return true;
    }
    catch (const std::exception &exception)
    {
        close_model();
        push_log(LogLevel::Error, exception.what());
        return false;
    }
}

bool EditorState::reopen_original()
{
    if (original_path_.empty())
    {
        push_log(LogLevel::Warning, "No original model to reopen.");
        return false;
    }
    return open_model(original_path_);
}

void EditorState::close_model()
{
    document_.reset();
    selection_ = {};
    original_path_.clear();
    current_path_.clear();
    undo_.reset();
    unsaved_changes_ = false;
    valid_ = false;
}

bool EditorState::save()
{
    const auto target = current_path_.empty() ? default_save_path() : current_path_;
    return save_to_path(target, true);
}

bool EditorState::save_as(const std::filesystem::path &path)
{
    return save_to_path(path, true);
}

bool EditorState::validate_model()
{
    if (!document_)
    {
        push_log(LogLevel::Warning, "No model is open.");
        return false;
    }

    try
    {
        mdl::MdlValidator::validate(*document_);
        valid_ = true;
        push_log(LogLevel::Success, "Model validation passed.");
        return true;
    }
    catch (const std::exception &exception)
    {
        valid_ = false;
        push_log(LogLevel::Error, exception.what());
        return false;
    }
}

bool EditorState::add_skin(const AddSkinRequest &request)
{
    if (!document_)
    {
        push_log(LogLevel::Warning, "No model is open.");
        return false;
    }
    if (request.submodel_index < 0 || request.submodel_index >= static_cast<int>(document_->submodels.size()))
    {
        push_log(LogLevel::Error, "Invalid submodel selection.");
        return false;
    }
    const auto &submodel = document_->submodels[static_cast<std::size_t>(request.submodel_index)];
    if (request.mesh_index < 0 || request.mesh_index >= static_cast<int>(submodel.meshes.size()))
    {
        push_log(LogLevel::Error, "Invalid mesh selection.");
        return false;
    }

    mdl::AddSkinOptions options;
    options.model_path = original_path_;
    options.bmp_path = request.bmp_path;
    options.output_path = default_save_path();
    options.submodel_index = request.submodel_index;
    options.copy_skin_index = request.copy_skin_family;
    options.target_texture = submodel.meshes[static_cast<std::size_t>(request.mesh_index)].family0_texture_name;

    try
    {
        const auto result = mdl::SkinFamilyEditor::add_skin(*document_, options);
        undo_ = UndoRecord{
            UndoType::AddSkin,
            result.new_skin_index,
            result.texture_index,
            result.texture_added,
            document_->skin_families.back(),
            {}
        };
        selection_.submodel_index = request.submodel_index;
        selection_.mesh_index = request.mesh_index;
        selection_.skin_family_index = result.new_skin_index;
        selection_.texture_index = result.texture_index;
        unsaved_changes_ = true;

        push_log(
            LogLevel::Success,
            "Skin successfully added. Texture index: " + std::to_string(result.texture_index) +
                ", skin index: " + std::to_string(result.new_skin_index) +
                ", reference: " + make_reference(request.submodel_index, result.new_skin_index));
        return true;
    }
    catch (const std::exception &exception)
    {
        push_log(LogLevel::Error, exception.what());
        return false;
    }
}

bool EditorState::remove_skin_family(int skin_index)
{
    if (!document_)
    {
        push_log(LogLevel::Warning, "No model is open.");
        return false;
    }

    try
    {
        const auto result = mdl::SkinFamilyEditor::remove_skin_family(*document_, skin_index);
        undo_ = UndoRecord{UndoType::RemoveSkin, result.removed_skin_index, -1, false, result.removed_family, {}};
        if (selection_.skin_family_index >= static_cast<int>(document_->skin_families.size()))
        {
            selection_.skin_family_index = static_cast<int>(document_->skin_families.size()) - 1;
        }
        unsaved_changes_ = true;
        push_log(LogLevel::Warning, "Removed skin family " + std::to_string(skin_index) + ".");
        return true;
    }
    catch (const std::exception &exception)
    {
        push_log(LogLevel::Error, exception.what());
        return false;
    }
}

bool EditorState::replace_texture(const ReplaceTextureRequest &request)
{
    if (!document_)
    {
        push_log(LogLevel::Warning, "No model is open.");
        return false;
    }

    try
    {
        const auto result =
            mdl::SkinFamilyEditor::replace_texture(*document_, request.texture_index, request.bmp_path, request.keep_name);
        undo_ = UndoRecord{UndoType::ReplaceTexture, -1, request.texture_index, false, {}, result.previous_texture};
        selection_.texture_index = request.texture_index;
        unsaved_changes_ = true;
        refresh_cached_views();
        push_log(
            LogLevel::Success,
            "Replaced texture " + std::to_string(request.texture_index) + " (" +
                document_->textures[static_cast<std::size_t>(request.texture_index)].name() + ").");
        return true;
    }
    catch (const std::exception &exception)
    {
        push_log(LogLevel::Error, exception.what());
        return false;
    }
}

bool EditorState::undo_last_operation()
{
    if (!document_ || !undo_.has_value())
    {
        push_log(LogLevel::Warning, "Nothing to undo.");
        return false;
    }

    switch (undo_->type)
    {
    case UndoType::AddSkin:
        if (undo_->skin_index >= 0 &&
            undo_->skin_index < static_cast<int>(document_->skin_families.size()))
        {
            document_->skin_families.erase(document_->skin_families.begin() + undo_->skin_index);
        }
        if (undo_->texture_added && !document_->textures.empty() &&
            undo_->texture_index == static_cast<int>(document_->textures.size()) - 1)
        {
            document_->textures.pop_back();
        }
        push_log(LogLevel::Info, "Undid last add-skin operation.");
        break;
    case UndoType::RemoveSkin:
        document_->skin_families.insert(
            document_->skin_families.begin() + undo_->skin_index,
            undo_->family);
        push_log(LogLevel::Info, "Restored removed skin family.");
        break;
    case UndoType::ReplaceTexture:
        document_->textures[static_cast<std::size_t>(undo_->texture_index)] = undo_->previous_texture;
        refresh_cached_views();
        push_log(LogLevel::Info, "Restored previous texture data.");
        break;
    default:
        return false;
    }

    undo_.reset();
    unsaved_changes_ = true;
    return true;
}

bool EditorState::has_document() const
{
    return static_cast<bool>(document_);
}

bool EditorState::has_unsaved_changes() const
{
    return unsaved_changes_;
}

bool EditorState::is_valid() const
{
    return valid_;
}

const mdl::MdlDocument *EditorState::document() const
{
    return document_.get();
}

mdl::MdlDocument *EditorState::document()
{
    return document_.get();
}

const SelectionState &EditorState::selection() const
{
    return selection_;
}

SelectionState &EditorState::selection()
{
    return selection_;
}

const std::vector<LogEntry> &EditorState::log() const
{
    return log_;
}

const std::filesystem::path &EditorState::original_path() const
{
    return original_path_;
}

const std::filesystem::path &EditorState::current_path() const
{
    return current_path_;
}

bool EditorState::backup_enabled() const
{
    return backup_enabled_;
}

void EditorState::set_backup_enabled(bool enabled)
{
    backup_enabled_ = enabled;
}

void EditorState::clear_log()
{
    log_.clear();
}

std::string EditorState::copyable_log() const
{
    std::ostringstream stream;
    for (const auto &entry : log_)
    {
        stream << entry.message << '\n';
    }
    return stream.str();
}

std::string EditorState::effective_reference_name() const
{
    const auto &path = current_path_.empty() ? default_save_path() : current_path_;
    return filename_or_path(path);
}

std::string EditorState::make_reference(int submodel_index, int skin_index) const
{
    return effective_reference_name() + ":" + std::to_string(submodel_index) + ":" + std::to_string(skin_index);
}

std::string EditorState::status_line() const
{
    if (!document_)
    {
        return "No model loaded";
    }

    std::ostringstream stream;
    stream << filename_or_path(current_path_.empty() ? original_path_ : current_path_);
    if (unsaved_changes_)
    {
        stream << "* | Unsaved changes";
    }
    stream << " | MDL v" << document_->header.version
           << " | " << document_->submodels.size() << " submodels"
           << " | " << document_->textures.size() << " textures"
           << " | " << document_->skin_families.size() << " skins"
           << " | " << (valid_ ? "Valid" : "Invalid");
    return stream.str();
}

std::vector<std::string> EditorState::texture_usage(int texture_index) const
{
    if (!document_)
    {
        return {};
    }
    return mdl::SkinFamilyEditor::describe_texture_usage(*document_, texture_index);
}

void EditorState::refresh_cached_views()
{
    if (!document_ || document_->skin_families.empty())
    {
        return;
    }
    const auto &family0 = document_->skin_families.front();
    for (auto &submodel : document_->submodels)
    {
        for (auto &mesh : submodel.meshes)
        {
            if (mesh.mesh.skinref >= 0 && mesh.mesh.skinref < static_cast<int>(family0.size()))
            {
                mesh.family0_texture_index = family0[static_cast<std::size_t>(mesh.mesh.skinref)];
                if (mesh.family0_texture_index >= 0 &&
                    mesh.family0_texture_index < static_cast<int>(document_->textures.size()))
                {
                    mesh.family0_texture_name =
                        document_->textures[static_cast<std::size_t>(mesh.family0_texture_index)].name();
                }
            }
        }
    }
}

bool EditorState::save_to_path(const std::filesystem::path &path, bool update_current_path)
{
    if (!document_)
    {
        push_log(LogLevel::Warning, "No model is open.");
        return false;
    }

    try
    {
        const auto temp_path = temp_save_path(path);
        mdl::MdlWriter::write(*document_, temp_path);
        const auto reloaded = mdl::MdlReader::read(temp_path);
        mdl::MdlValidator::validate(reloaded);

        if (std::filesystem::exists(path))
        {
            if (backup_enabled_)
            {
                std::filesystem::copy_file(
                    path,
                    backup_path_for(path),
                    std::filesystem::copy_options::overwrite_existing);
            }
            std::filesystem::remove(path);
        }

        std::filesystem::rename(temp_path, path);
        if (update_current_path)
        {
            current_path_ = path;
        }
        valid_ = true;
        unsaved_changes_ = false;
        push_log(LogLevel::Success, "Saved model: " + path_to_utf8(path));
        return true;
    }
    catch (const std::exception &exception)
    {
        push_log(LogLevel::Error, exception.what());
        return false;
    }
}

void EditorState::push_log(LogLevel level, std::string message)
{
    log_.push_back(LogEntry{level, std::move(message)});
}

std::filesystem::path EditorState::default_save_path() const
{
    if (!current_path_.empty())
    {
        return current_path_;
    }
    auto path = original_path_;
    path.replace_filename(path.stem().string() + "_new" + path.extension().string());
    return path;
}

std::filesystem::path EditorState::temp_save_path(const std::filesystem::path &target)
{
    return target.string() + ".tmp";
}

std::filesystem::path EditorState::backup_path_for(const std::filesystem::path &target)
{
    return target.string() + ".bak";
}

void EditorState::reset_undo()
{
    undo_.reset();
}
} // namespace gui
