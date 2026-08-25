#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "dialogs/add_skin_dialog.hpp"
#include "dialogs/replace_texture_dialog.hpp"
#include "editor_state.hpp"
#include "texture_preview.hpp"
#include "windows/inspector_window.hpp"
#include "windows/log_window.hpp"
#include "windows/model_tree_window.hpp"
#include "windows/skins_window.hpp"
#include "windows/textures_window.hpp"

struct GLFWwindow;

namespace gui
{
class Application
{
public:
    Application();
    ~Application();

    bool initialize();
    int run(const std::optional<std::filesystem::path> &startup_model);

private:
    struct Settings
    {
        bool show_model_tree = true;
        bool show_skins = true;
        bool show_textures = true;
        bool show_inspector = true;
        bool show_log = true;
        bool backup_enabled = true;
        int window_width = 1600;
        int window_height = 900;
        std::filesystem::path last_model_dir;
        std::filesystem::path last_bmp_dir;
    };

    enum class PendingAction
    {
        None,
        Exit,
        CloseModel
    };

    void shutdown();
    void render_frame();
    void draw_main_menu();
    void draw_status_bar();
    void draw_popups();
    void try_open_startup_model(const std::optional<std::filesystem::path> &startup_model);
    bool request_open_model();
    bool request_save_as();
    std::filesystem::path open_model_dialog();
    std::filesystem::path open_bmp_dialog();
    std::filesystem::path save_model_dialog();
    void request_action(PendingAction action);
    void perform_pending_action(bool discard_changes);
    void load_settings();
    void save_settings() const;
    std::filesystem::path settings_directory() const;
    static void drop_callback(GLFWwindow *window, int count, const char **paths);
    void handle_drop(int count, const char **paths);
    void load_font();

    GLFWwindow *window_ = nullptr;
    EditorState state_;
    TexturePreview preview_;
    ModelTreeWindow model_tree_window_;
    SkinsWindow skins_window_;
    TexturesWindow textures_window_;
    InspectorWindow inspector_window_;
    LogWindow log_window_;
    AddSkinDialog add_skin_dialog_;
    ReplaceTextureDialog replace_texture_dialog_;
    Settings settings_{};
    PendingAction pending_action_ = PendingAction::None;
    bool show_about_ = false;
};
} // namespace gui
