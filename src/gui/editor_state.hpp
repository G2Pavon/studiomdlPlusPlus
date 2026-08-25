#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mdl/mdl_document.hpp"
#include "mdl/skin_family_editor.hpp"

namespace gui
{
enum class LogLevel
{
    Info,
    Warning,
    Error,
    Success
};

struct LogEntry
{
    LogLevel level = LogLevel::Info;
    std::string message;
};

struct SelectionState
{
    int submodel_index = -1;
    int mesh_index = -1;
    int texture_index = -1;
    int skin_family_index = -1;
    int skinref_index = -1;
};

struct AddSkinRequest
{
    int submodel_index = -1;
    int mesh_index = -1;
    int copy_skin_family = 0;
    std::filesystem::path bmp_path;
};

struct ReplaceTextureRequest
{
    int texture_index = -1;
    std::filesystem::path bmp_path;
    bool keep_name = true;
};

class EditorState
{
public:
    EditorState();

    bool open_model(const std::filesystem::path &path);
    bool reopen_original();
    void close_model();

    bool save();
    bool save_as(const std::filesystem::path &path);
    bool validate_model();

    bool add_skin(const AddSkinRequest &request);
    bool remove_skin_family(int skin_index);
    bool replace_texture(const ReplaceTextureRequest &request);
    bool undo_last_operation();

    [[nodiscard]] bool has_document() const;
    [[nodiscard]] bool has_unsaved_changes() const;
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] const mdl::MdlDocument *document() const;
    [[nodiscard]] mdl::MdlDocument *document();
    [[nodiscard]] const SelectionState &selection() const;
    [[nodiscard]] SelectionState &selection();
    [[nodiscard]] const std::vector<LogEntry> &log() const;
    [[nodiscard]] const std::filesystem::path &original_path() const;
    [[nodiscard]] const std::filesystem::path &current_path() const;
    [[nodiscard]] bool backup_enabled() const;
    void set_backup_enabled(bool enabled);
    void clear_log();
    std::string copyable_log() const;
    std::string effective_reference_name() const;
    std::string make_reference(int submodel_index, int skin_index) const;
    std::string status_line() const;
    std::vector<std::string> texture_usage(int texture_index) const;
    void refresh_cached_views();

private:
    enum class UndoType
    {
        None,
        AddSkin,
        RemoveSkin,
        ReplaceTexture
    };

    struct UndoRecord
    {
        UndoType type = UndoType::None;
        int skin_index = -1;
        int texture_index = -1;
        bool texture_added = false;
        std::vector<std::int16_t> family;
        mdl::TextureData previous_texture;
    };

    bool save_to_path(const std::filesystem::path &path, bool update_current_path);
    void push_log(LogLevel level, std::string message);
    std::filesystem::path default_save_path() const;
    static std::filesystem::path temp_save_path(const std::filesystem::path &target);
    static std::filesystem::path backup_path_for(const std::filesystem::path &target);
    void reset_undo();

    std::unique_ptr<mdl::MdlDocument> document_;
    SelectionState selection_{};
    std::vector<LogEntry> log_;
    std::filesystem::path original_path_;
    std::filesystem::path current_path_;
    std::optional<UndoRecord> undo_;
    bool unsaved_changes_ = false;
    bool valid_ = false;
    bool backup_enabled_ = true;
};
} // namespace gui
