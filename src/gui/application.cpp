#include "application.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "GLFW/glfw3.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "gui_strings.hpp"
#include "gui_theme.hpp"
#include "nfd.h"
#include "utils/cmdlib.hpp"

namespace gui
{
namespace
{
std::filesystem::path appdata_path()
{
#ifdef _WIN32
    const char *appdata = std::getenv("APPDATA");
    if (appdata != nullptr)
    {
        return std::filesystem::path(appdata);
    }
#endif
    const char *home = std::getenv("HOME");
    if (home != nullptr)
    {
        return std::filesystem::path(home) / ".config";
    }
    return std::filesystem::temp_directory_path();
}

bool parse_bool(const std::string &value)
{
    return value == "1" || value == "true" || value == "TRUE";
}
} // namespace

Application::Application() = default;

Application::~Application()
{
    shutdown();
}

bool Application::initialize()
{
    load_settings();
    if (glfwInit() == 0)
    {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    window_ = glfwCreateWindow(
        settings_.window_width,
        settings_.window_height,
        strings::kAppTitle,
        nullptr,
        nullptr);
    if (window_ == nullptr)
    {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(window_, this);
    glfwSetDropCallback(window_, &Application::drop_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    const auto imgui_ini = settings_directory() / "imgui.ini";
    static std::string ini_path = imgui_ini.string();
    io.IniFilename = ini_path.c_str();
    load_font();
    apply_gui_theme();

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true))
    {
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 130"))
    {
        return false;
    }

    NFD_Init();
    state_.set_backup_enabled(settings_.backup_enabled);
    return true;
}

int Application::run(const std::optional<std::filesystem::path> &startup_model)
{
    if (!initialize())
    {
        return 1;
    }

    try_open_startup_model(startup_model);

    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        render_frame();
        glfwSwapBuffers(window_);
    }

    save_settings();
    return 0;
}

void Application::shutdown()
{
    preview_.clear();
    if (window_ != nullptr)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        NFD_Quit();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

void Application::render_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport();
    draw_main_menu();

    if (settings_.show_model_tree)
    {
        model_tree_window_.draw(state_);
    }
    if (settings_.show_skins)
    {
        skins_window_.draw(state_);
    }
    if (settings_.show_textures)
    {
        textures_window_.draw(state_);
    }
    if (settings_.show_inspector)
    {
        inspector_window_.draw(state_, preview_);
    }
    if (settings_.show_log)
    {
        log_window_.draw(state_);
    }

    add_skin_dialog_.draw(state_, [this]() { return open_bmp_dialog(); });
    replace_texture_dialog_.draw(state_, [this]() { return open_bmp_dialog(); });
    draw_status_bar();
    draw_popups();

    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::draw_main_menu()
{
    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open model"))
        {
            request_open_model();
        }
        if (ImGui::MenuItem("Save", nullptr, false, state_.has_document()))
        {
            state_.save();
        }
        if (ImGui::MenuItem("Save as", nullptr, false, state_.has_document()))
        {
            request_save_as();
        }
        if (ImGui::MenuItem("Close model", nullptr, false, state_.has_document()))
        {
            request_action(PendingAction::CloseModel);
        }
        if (ImGui::MenuItem("Exit"))
        {
            request_action(PendingAction::Exit);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Add skin", nullptr, false, state_.has_document()))
        {
            add_skin_dialog_.open(state_);
        }
        if (ImGui::MenuItem("Replace texture", nullptr, false, state_.has_document()))
        {
            replace_texture_dialog_.open(state_);
        }
        if (ImGui::MenuItem("Remove skin", nullptr, false, state_.has_document() && state_.selection().skin_family_index > 0))
        {
            ImGui::OpenPopup(strings::kPopupRemoveSkin);
        }
        ImGui::BeginDisabled(true);
        ImGui::MenuItem("Remove unused textures", nullptr, false, false);
        ImGui::EndDisabled();
        if (ImGui::MenuItem("Undo last operation", nullptr, false, state_.has_document()))
        {
            state_.undo_last_operation();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Model"))
    {
        if (ImGui::MenuItem("Validate", nullptr, false, state_.has_document()))
        {
            state_.validate_model();
        }
        if (ImGui::MenuItem("Model information", nullptr, false, state_.has_document()))
        {
            settings_.show_inspector = true;
        }
        if (ImGui::MenuItem("Reopen original", nullptr, false, state_.has_document()))
        {
            state_.reopen_original();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Model tree", nullptr, &settings_.show_model_tree);
        ImGui::MenuItem("Skin families", nullptr, &settings_.show_skins);
        ImGui::MenuItem("Textures", nullptr, &settings_.show_textures);
        ImGui::MenuItem("Inspector", nullptr, &settings_.show_inspector);
        ImGui::MenuItem("Log", nullptr, &settings_.show_log);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("About"))
        {
            show_about_ = true;
            ImGui::OpenPopup(strings::kDialogAbout);
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Application::draw_status_bar()
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 28.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 28.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##status_bar", nullptr, flags))
    {
        ImGui::TextUnformatted(state_.status_line().c_str());
    }
    ImGui::End();
}

void Application::draw_popups()
{
    if (show_about_ && ImGui::BeginPopupModal(strings::kDialogAbout, &show_about_, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("mdltool_gui");
        ImGui::TextUnformatted("GoldSrc MDL v10 editor built on mdl_core.");
        ImGui::TextUnformatted("Texture preview only. No 3D model viewport in this version.");
        if (ImGui::Button("Close"))
        {
            show_about_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(strings::kPopupUnsaved, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("The model has unsaved changes.");
        if (ImGui::Button("Save"))
        {
            if (state_.save())
            {
                perform_pending_action(false);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            perform_pending_action(true);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            pending_action_ = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(strings::kPopupRemoveSkin, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const int skin_index = state_.selection().skin_family_index;
        ImGui::Text("Remove skin family %d?", skin_index);
        ImGui::TextUnformatted("This operation changes skin indexes after the removed family.");
        if (ImGui::Button("Remove"))
        {
            state_.remove_skin_family(skin_index);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Application::try_open_startup_model(const std::optional<std::filesystem::path> &startup_model)
{
    if (startup_model.has_value())
    {
        state_.open_model(*startup_model);
    }
}

bool Application::request_open_model()
{
    const auto path = open_model_dialog();
    if (path.empty())
    {
        return false;
    }
    settings_.last_model_dir = path.parent_path();
    return state_.open_model(path);
}

bool Application::request_save_as()
{
    const auto path = save_model_dialog();
    if (path.empty())
    {
        return false;
    }
    settings_.last_model_dir = path.parent_path();
    return state_.save_as(path);
}

std::filesystem::path Application::open_model_dialog()
{
    nfdu8char_t *out_path = nullptr;
    nfdu8filteritem_t filters[1] = {{"GoldSrc MDL", "mdl"}};
    nfdopendialogu8args_t args{};
    args.filterList = filters;
    args.filterCount = 1;
    const std::string default_path = settings_.last_model_dir.empty() ? std::string() : settings_.last_model_dir.string();
    args.defaultPath = default_path.empty() ? nullptr : default_path.c_str();
    const auto result = NFD_OpenDialogU8_With(&out_path, &args);
    if (result == NFD_OKAY)
    {
        std::filesystem::path path(out_path);
        NFD_FreePathU8(out_path);
        return path;
    }
    return {};
}

std::filesystem::path Application::open_bmp_dialog()
{
    nfdu8char_t *out_path = nullptr;
    nfdu8filteritem_t filters[1] = {{"Indexed BMP", "bmp"}};
    nfdopendialogu8args_t args{};
    args.filterList = filters;
    args.filterCount = 1;
    const std::string default_path = settings_.last_bmp_dir.empty() ? std::string() : settings_.last_bmp_dir.string();
    args.defaultPath = default_path.empty() ? nullptr : default_path.c_str();
    const auto result = NFD_OpenDialogU8_With(&out_path, &args);
    if (result == NFD_OKAY)
    {
        std::filesystem::path path(out_path);
        settings_.last_bmp_dir = path.parent_path();
        NFD_FreePathU8(out_path);
        return path;
    }
    return {};
}

std::filesystem::path Application::save_model_dialog()
{
    nfdu8char_t *out_path = nullptr;
    nfdu8filteritem_t filters[1] = {{"GoldSrc MDL", "mdl"}};
    nfdsavedialogu8args_t args{};
    args.filterList = filters;
    args.filterCount = 1;
    const auto default_path = state_.current_path().empty() ? state_.original_path() : state_.current_path();
    const std::string default_name = default_path.empty() ? std::string("model_new.mdl") : default_path.filename().string();
    const std::string default_dir = default_path.empty() ? std::string() : default_path.parent_path().string();
    args.defaultName = default_name.c_str();
    args.defaultPath = default_dir.empty() ? nullptr : default_dir.c_str();
    const auto result = NFD_SaveDialogU8_With(&out_path, &args);
    if (result == NFD_OKAY)
    {
        std::filesystem::path path(out_path);
        NFD_FreePathU8(out_path);
        return path;
    }
    return {};
}

void Application::request_action(PendingAction action)
{
    if (state_.has_unsaved_changes())
    {
        pending_action_ = action;
        ImGui::OpenPopup(strings::kPopupUnsaved);
        return;
    }
    pending_action_ = action;
    perform_pending_action(true);
}

void Application::perform_pending_action(bool discard_changes)
{
    if (!discard_changes && state_.has_unsaved_changes())
    {
        return;
    }

    switch (pending_action_)
    {
    case PendingAction::Exit:
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        break;
    case PendingAction::CloseModel:
        state_.close_model();
        preview_.clear();
        break;
    case PendingAction::None:
    default:
        break;
    }
    pending_action_ = PendingAction::None;
}

void Application::load_settings()
{
    const auto path = settings_directory() / "settings.ini";
    std::ifstream file(path);
    if (!file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        const auto pos = line.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }
        const auto key = line.substr(0, pos);
        const auto value = line.substr(pos + 1);
        if (key == "backup")
        {
            settings_.backup_enabled = parse_bool(value);
        }
        else if (key == "show_log")
        {
            settings_.show_log = parse_bool(value);
        }
        else if (key == "last_model_dir")
        {
            settings_.last_model_dir = value;
        }
        else if (key == "last_bmp_dir")
        {
            settings_.last_bmp_dir = value;
        }
        else if (key == "window_width")
        {
            settings_.window_width = std::stoi(value);
        }
        else if (key == "window_height")
        {
            settings_.window_height = std::stoi(value);
        }
    }
}

void Application::save_settings() const
{
    const auto dir = settings_directory();
    std::filesystem::create_directories(dir);
    std::ofstream file(dir / "settings.ini");
    if (!file.is_open())
    {
        return;
    }
    int width = settings_.window_width;
    int height = settings_.window_height;
    if (window_ != nullptr)
    {
        glfwGetWindowSize(window_, &width, &height);
    }
    file << "backup=" << (state_.backup_enabled() ? "1" : "0") << '\n';
    file << "show_log=" << (settings_.show_log ? "1" : "0") << '\n';
    file << "last_model_dir=" << settings_.last_model_dir.string() << '\n';
    file << "last_bmp_dir=" << settings_.last_bmp_dir.string() << '\n';
    file << "window_width=" << width << '\n';
    file << "window_height=" << height << '\n';
}

std::filesystem::path Application::settings_directory() const
{
    return appdata_path() / "mdltool_gui";
}

void Application::drop_callback(GLFWwindow *window, int count, const char **paths)
{
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    if (app != nullptr)
    {
        app->handle_drop(count, paths);
    }
}

void Application::handle_drop(int count, const char **paths)
{
    if (count <= 0 || paths == nullptr)
    {
        return;
    }
    const std::filesystem::path path(paths[0]);
    if (path.extension() == ".mdl")
    {
        state_.open_model(path);
    }
}

void Application::load_font()
{
    auto &io = ImGui::GetIO();
    const ImWchar *ranges = io.Fonts->GetGlyphRangesCyrillic();
    std::vector<std::filesystem::path> candidates = {
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
#endif
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };

    for (const auto &path : candidates)
    {
        if (std::filesystem::exists(path))
        {
            ImFontConfig config{};
            config.OversampleH = 2;
            config.OversampleV = 2;
            if (io.Fonts->AddFontFromFileTTF(path.string().c_str(), 17.0f, &config, ranges) != nullptr)
            {
                return;
            }
        }
    }

    io.Fonts->AddFontDefault();
}
} // namespace gui
