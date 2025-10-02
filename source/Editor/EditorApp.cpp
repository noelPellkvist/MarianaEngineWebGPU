#include <Editor/EditorApp.hpp>
#include <FileReader.hpp>
#include <Editor/EditorCameraController.hpp>
#include <AssetManager.hpp>


#include <sstream>
#include <imgui.h>
#include <unordered_map>
#include <filesystem>
#include <algorithm>

#pragma region Helpers

static bool DragOrInputFloat(const char* id, float* v, float speed, const char* fmt, float width)
{
    struct State { bool editing = false; };
    static std::unordered_map<ImGuiID, State> s;

    ImGuiID iid = ImGui::GetID(id);
    State& st = s[iid];
    bool changed = false;

    ImGui::SetNextItemWidth(width);

    if (!st.editing)
    {
        changed |= ImGui::DragFloat(id, v, speed, 0.0f, 0.0f, fmt);

        // Click without dragging -> switch to text mode
        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
        {
            st.editing = true;
            ImGui::SetKeyboardFocusHere(0);
        }
    }
    else
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), fmt, static_cast<double>(*v));

        bool submit = ImGui::InputText(id, buf, IM_ARRAYSIZE(buf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (submit || ImGui::IsItemDeactivated())
        {
            *v = std::strtof(buf, nullptr);
            st.editing = false;
            changed = true;
        }
    }

    return changed;
}

inline std::string ToString(const glm::vec3& v)
{
    std::ostringstream ss;
    ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return ss.str();
}

inline std::string ToString(const float& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

#pragma endregion

struct UBO {
  glm::vec3 lightDir;
};

UBO ubo{};
UniformLayout uboLayout(false, ubo, ubo.lightDir);

struct TransformData {
  glm::mat4x4 modelMatrix;
  glm::mat4x4 normalMatrix;
};

TransformData transformBuffer{};
UniformLayout transformLayout(true, transformBuffer, transformBuffer.modelMatrix, transformBuffer.normalMatrix);

GLTF::GLTFMaterialProperties materialsBuffer;
UniformLayout materialsLayout(true, materialsBuffer, materialsBuffer.baseColor);

GLTF::Vertex v{};
VertexBufferLayout vertexLayout{v, v.position, v.normal, v.tangent, v.texcoord0, v.texcoord1, v.color0};

Entity avocado;

EditorApp::EditorApp(const std::string& name) : Application(name), 
renderpass(true, true, m_Window.GetWindowFormat(), m_Window.GetWidth(), m_Window.GetHeight())
{
    cam = new EditorCameraController(input);
    PBR_Shader = std::make_unique<Shader<UBO, TransformData, GLTF::GLTFMaterialProperties, CameraInfo>>(uboLayout, transformLayout, materialsLayout, cam->GetBinding(), vertexLayout, 5);
    AssetManager::LoadedShaders.push_back(PBR_Shader);
    
    avocado = scene.Instantiate("Avocado");

    auto s = scene.CreateSystem<LocalTRS, WorldXform>([](Entity ent, LocalTRS& trs, WorldXform& form, float dt){
        Logger::Warning("Running system for entity: " + std::string(ent.GetName()) + ToString(dt));
    });
    avocado.Add<RendererComponent>({0,0});

    scene.Update(0.f);
    m_Window.RegisterResizeCallback([this](int w, int h) {
        this->OnWindowResized(w, h);
    });
}

EditorApp::~EditorApp()
{

}

void EditorApp::OnWindowResized(int w, int h)
{
    m_Window.GetSurface();
    renderpass.Recreate(w, h);
}

void EditorApp::OnStart()
{
    Logger::Info("Starting");

    if (auto* editorCam = dynamic_cast<EditorCameraController*>(cam)) {
        editorCam->SetPosition({0.0f, 0.0f, 0.0f});
        editorCam->SetYawPitch(glm::half_pi<float>(), 0.0f);
    }

    renderpass.Init();
    PBR_Shader->LoadShader(FileReader::LoadRawString("/Shaders/test.wgsl"), {m_Window.GetWindowFormat()});
    GLTF::GLTFLoader::LoadGLTF(std::string(RESOURCE_DIR) + "/Models/DamagedHelmet.glb", *PBR_Shader);
    
    renderer.Init(scene);
    
    ubo.lightDir = glm::normalize(glm::vec3(1.0f, 0.5f, -1.0f));
    uboLayout.pack(ubo);

    LoadFileTextures();
}

void EditorApp::LoadFileTextures()
{
    Texture newTexture;
    newTexture.LoadTexture("/logo.png", TextureFormat::RGBA8UnormSrgb);
    AssetsTextures.push_back(newTexture);
}

void EditorApp::OnUpdate(float deltaTime)
{
    if(input.IsKeyPressed(Key::F11))
    {
        m_Window.ToggleFullscreen();
        m_Window.GetSurface();
        renderpass.Recreate(m_Window.GetWidth(), m_Window.GetHeight());
    }
    scene.Update(deltaTime);
}

void EditorApp::OnGUI()
{
    DrawTopMenu();
    ImGui::Begin("Stats"); 
    float fps   = ImGui::GetIO().Framerate;
    float ms    = 1000.0f / fps;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms, fps);
    gui.DrawTexture(AssetsTextures[0], 200, 200);
    ImGui::End();

    ImGui::Begin("Hierachy"); 
    scene.ForEachRoot([&](Entity e){
        DrawEntityNode(e);
    });
    ImGui::End();

    ImGui::Begin("Assets"); 
    DrawAssetsWindow();
    ImGui::End();

    ImGui::Begin("Inspector"); 
    if (selectedEntityID == -1)
    {
        ImGui::Text("Select an entity to show it here");
    }
    else
    {
        DrawInspector(selectedEntity);
        transformBuffer.modelMatrix = glm::make_mat4(selectedEntity.Get<WorldXform>()->model);
        transformBuffer.normalMatrix = glm::transpose(glm::inverse(glm::mat3(transformBuffer.modelMatrix)));
        transformLayout.pack(transformBuffer, 0);
    }
    ImGui::End();
}

#pragma region GUI

namespace fs = std::filesystem;



void EditorApp::DrawAssetsWindow()
{
    static const fs::path kRoot = fs::path(RESOURCE_DIR);
    static float tileSize  = 96.0f;
    static float labelH    = 24.0f;
    static float padding   = 8.0f;

    static fs::path current = fs::exists(kRoot) ? fs::absolute(kRoot) : fs::current_path();
    static std::string selectedPath;
    if (!fs::exists(current) || !fs::is_directory(current)) current = kRoot;

    // --- breadcrumbs (slim) ---
    const float crumbH = ImGui::GetFrameHeight();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,0));
    ImGui::BeginChild("##breadcrumbs",
                      ImVec2(0, crumbH),
                      false,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    std::string rootLabel = kRoot.filename().empty() ? kRoot.string() : kRoot.filename().string();
    if (ImGui::SmallButton(rootLabel.c_str())) { current = kRoot; selectedPath.clear(); }

    fs::path rel;
    try { rel = fs::relative(current, kRoot); } catch(...) { rel.clear(); }

    fs::path accum = kRoot;
    for (auto &part : rel) {
        if (part.empty() || part == ".") continue;
        ImGui::SameLine(); ImGui::TextUnformatted("\uf054"); ImGui::SameLine(); // chevron (optional)
        std::string seg = part.string();
        if (ImGui::SmallButton(seg.c_str())) { accum /= part; current = accum; selectedPath.clear(); }
        else { accum /= part; }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::Separator();

    // Collect entries
    struct Entry { fs::path p; bool isDir; };
    std::vector<Entry> items;
    try {
        for (auto &e : fs::directory_iterator(current)) items.push_back({ e.path(), e.is_directory() });
    } catch(...) {}

    std::sort(items.begin(), items.end(), [](const Entry& a, const Entry& b){
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.p.filename().string() < b.p.filename().string();
    });

    // Attempt to find the icon font (assumes you added it after main font)
    static ImFont* s_iconFont = nullptr;
    if (!s_iconFont) {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.Fonts->Fonts.empty()) {
            s_iconFont = io.Fonts->Fonts.back();
        }
    }

    ImGui::BeginChild("##grid", ImVec2(0,0), true, ImGuiWindowFlags_HorizontalScrollbar);

    const float cellW = tileSize + padding*2.0f;
    const float cellH = tileSize + labelH + padding*2.0f;
    const float availX = ImGui::GetContentRegionAvail().x;
    int columns = (int)std::max(1.0f, floorf(availX / cellW));

    if (ImGui::BeginTable("##grid_table", columns, ImGuiTableFlags_SizingFixedFit)) {
        int col = 0;
        for (size_t i=0; i<items.size(); ++i) {
            if (col == 0) ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(col);

            const Entry& en = items[i];
            ImGui::PushID((int)i);

            bool selected = (!selectedPath.empty() && selectedPath == en.p.string());

            // Invisible button to capture clicks/double-clicks
            ImGui::InvisibleButton("tile", ImVec2(cellW, cellH));
            bool clicked  = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool dblClick = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();

            // Draw subtle border only (no opaque background)
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_Border);
            dl->AddRect(rMin, rMax, borderCol, 6.0f);

            // faint selected overlay if selected
            if (selected) {
                dl->AddRectFilled(rMin, rMax, ImGui::GetColorU32(ImGuiCol_Header, 0.08f), 6.0f);
            }

            // Thumbnail area (we keep it visually empty so icon stands out)
            ImVec2 thumbMin = { rMin.x + padding, rMin.y + padding };
            ImVec2 thumbMax = { rMax.x - padding, rMin.y + padding + tileSize };

            // If folder, draw a *large* icon centered in the thumbnail
            if (en.isDir) {
                // Folder icon glyph
                const char* folderGlyph = "\uf07b"; 
                float glyphSize = tileSize * 1.25f; 

                // Measure glyph size at this scale
                ImVec2 glyphSz = s_iconFont
                    ? s_iconFont->CalcTextSizeA(glyphSize, FLT_MAX, 0.0f, folderGlyph)
                    : ImGui::CalcTextSize(folderGlyph);

                // Compute centered position
                float thumbW = thumbMax.x - thumbMin.x;
                float thumbH = thumbMax.y - thumbMin.y;
                float glyphX = thumbMin.x + (thumbW - glyphSz.x) * 0.5f;
                float glyphY = thumbMin.y + (thumbH - glyphSz.y) * 0.5f;

                // Small manual tweak for better horizontal centering (depends on font)
                glyphX -= glyphSize * 0.125f; // shift left ~5% of glyph size

                // Draw
                if (s_iconFont) {
                    dl->AddText(s_iconFont, glyphSize, ImVec2(glyphX, glyphY),
                                ImGui::GetColorU32(ImGuiCol_Text), folderGlyph);
                } else {
                    ImGui::SetCursorScreenPos(ImVec2(glyphX, glyphY));
                    ImGui::TextUnformatted("[DIR]");
                }

            }

            // Label centered under thumbnail
            std::string name = en.p.filename().string();
            ImVec2 textSz = ImGui::CalcTextSize(name.c_str(), nullptr, true, cellW - padding*2.0f);
            float textX = rMin.x + (cellW - textSz.x) * 0.5f;
            float textY = thumbMax.y + (labelH - textSz.y) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(textX, textY));
            ImGui::PushTextWrapPos(rMin.x + cellW - padding);
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopTextWrapPos();

            // Click behavior
            if (clicked) selectedPath = en.p.string();
            if (dblClick) {
                if (en.isDir) { current = en.p; selectedPath.clear(); }
                else {
                    // file open callback can go here
                }
            }

            ImGui::PopID();
            col = (col + 1) % columns;
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void EditorApp::DrawEntityNode(Entity& e)
{
    const char* name = e.GetName();
    if (!name || !*name) name = "<error_name>";

    ImGuiTreeNodeFlags flags = 
        ImGuiTreeNodeFlags_OpenOnArrow | 
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        (e.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf) |
        (selectedEntityID == e.RawId() ? ImGuiTreeNodeFlags_Selected : 0);
    ImGui::PushID((ImGuiID)(uintptr_t)e.RawId());
    bool open = ImGui::TreeNodeEx("label", flags, "%s", name);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)  && !ImGui::IsItemToggledOpen()) {
        selectedEntityID = e.RawId();
        selectedEntity = e;
    }

    if (ImGui::BeginPopupContextItem("entity_ctx")) {
        if (ImGui::MenuItem("Select")) selectedEntityID = e.RawId();
        ImGui::EndPopup();
    }

    if (open)
    {
        scene.ForEachChild(e, [&](Entity c){
            DrawEntityNode(c);
        });
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void EditorApp::DrawInspector(Entity& e)
{
    ImGui::TextDisabled("Name");
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(-1);

    char entityName[128] = {};
    strcpy(entityName, e.GetName());
    ImGui::InputText("##entity_name", entityName, 128);

    ImGui::Separator();
    
    const char* header = "⚙  Transform";
    if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
    {
        LocalTRS L_TRS = *(e.Get<LocalTRS>());
        float* pos   = L_TRS.pos;
        float* rotDeg = L_TRS.rot_euler;
        float* scl   = L_TRS.scl;

        if (ImGui::BeginTable("##transform_table", 2, ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoBordersInBody))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("values", ImGuiTableColumnFlags_WidthStretch);

            auto DrawVec3Row = [](const char* label, float v[3], float resetX, float resetY, float resetZ, float speed = 0.1f)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
            
                ImGui::PushID(label);
                float line_h = ImGui::GetFrameHeight();
                float btn_w  = line_h; // square reset buttons
                float full_w = ImGui::GetContentRegionAvail().x;
            
                // three equal fields (account for 3 buttons + inner spacing)
                float field_w = (full_w - btn_w*3.0f - ImGui::GetStyle().ItemInnerSpacing.x*6.0f) / 3.0f;
            
                // X
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 80, 80, 255));
                if (ImGui::Button("X", ImVec2(btn_w, line_h))) v[0] = resetX;
                ImGui::SameLine();
                DragOrInputFloat("##X", &v[0], speed, "%.3f", field_w);
                ImGui::PopStyleColor();
                ImGui::SameLine();
            
                // Y
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(110, 190, 110, 255));
                if (ImGui::Button("Y", ImVec2(btn_w, line_h))) v[1] = resetY;
                ImGui::SameLine();
                DragOrInputFloat("##Y", &v[1], speed, "%.3f", field_w);
                ImGui::PopStyleColor();
                ImGui::SameLine();
            
                // Z
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 140, 220, 255));
                if (ImGui::Button("Z", ImVec2(btn_w, line_h))) v[2] = resetZ;
                ImGui::SameLine();
                DragOrInputFloat("##Z", &v[2], speed, "%.3f", field_w);
                ImGui::PopStyleColor();
            
                ImGui::PopID();
            };


            DrawVec3Row("Position", pos,    0.0f, 0.0f, 0.0f, 0.1f);
            DrawVec3Row("Rotation", rotDeg, 0.0f, 0.0f, 0.0f, 0.5f);
            DrawVec3Row("Scale",    scl,    1.0f, 1.0f, 1.0f, 0.05f);

            e.SetPosition(pos[0], pos[1], pos[2]);
            e.SetRotationEuler(rotDeg[0], rotDeg[1], rotDeg[2]);
            e.SetScale(scl[0], scl[1], scl[2]);
            ImGui::EndTable();
        }
    }
    e.SetName(entityName);
}

void EditorApp::DrawMat4(const char* id, float m[16], bool editable, float speed, const char* fmt)
{
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit
                          | ImGuiTableFlags_Borders
                          | ImGuiTableFlags_RowBg
                          | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable(id, 5, flags))
    {
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("W", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 4));

        for (int r = 0; r < 4; ++r)
        {
            ImGui::TableNextRow();

            // Row label
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("r%d", r);

            for (int c = 0; c < 4; ++c)
            {
                ImGui::TableSetColumnIndex(c + 1);

                // Highlight the diagonal
                bool is_diag = (r == c);
                if (is_diag)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255)); // warm tint

                ImGui::PushID(r * 4 + c);
                float& v = m[r * 4 + c]; // <-- change to m[c*4 + r] if your data is column-major

                if (editable)
                {
                    // Right-aligned cells
                    float w = ImGui::GetContentRegionAvail().x;
                    ImGui::SetNextItemWidth(w);
                    ImGui::DragFloat("##cell", &v, speed, 0, 0, fmt);
                }
                else
                {
                    ImGui::Text(fmt, v);
                }

                ImGui::PopID();
                if (is_diag)
                    ImGui::PopStyleColor();
            }
        }

        ImGui::PopStyleVar();
        ImGui::EndTable();

        // Small legend
        ImGui::TextDisabled("Diagonal highlighted • %s", editable ? "editable" : "read-only");
    }
}

void EditorApp::DrawTopMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New", "Ctrl+N")) {/* TODO */}
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {/* TODO */}
            if (ImGui::MenuItem("Save", "Ctrl+S")) {/* TODO */}
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {/* TODO */}
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...", "Ctrl+,")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {/* set a flag to quit */}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {/* TODO */}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {/* TODO */}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {/* TODO */}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {/* TODO */}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {/* TODO */}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Documentation")) {/* open URL */}
            if (ImGui::MenuItem("Report Issue"))   {/* open URL */}
            ImGui::Separator();
            if (ImGui::MenuItem("About")) {}
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

#pragma endregion

void EditorApp::OnRender()
{
    float aspect = static_cast<float>(m_Window.GetWidth()) /
               static_cast<float>(m_Window.GetHeight());
    
    renderer.Render(renderpass, gui);
}

void EditorApp::OnShutdown()
{
    Logger::Info("Shutdown now");
}