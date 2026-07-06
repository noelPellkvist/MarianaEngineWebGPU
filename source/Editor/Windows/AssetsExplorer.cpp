#include "Editor/Windows/AssetsExplorer.hpp"

#include <imgui.h>
#include <filesystem>
#include <Logger.hpp>
#include <algorithm>


namespace fs = std::filesystem;

inline bool IsImageExtension(const std::string& ext) {
    if (ext.empty()) return false;
    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    // Common image extensions supported by stb_image
    static const std::vector<std::string> allowed = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".psd", ".hdr", ".pic", ".ppm"
    };
    return std::find(allowed.begin(), allowed.end(), e) != allowed.end();
}

AssetsExplorer::AssetsExplorer(GUI& gui)
    : EditorWindow("Assets", gui, true)
{
    LoadFileTextures();
}

void AssetsExplorer::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        DrawAssetsWindow();
    }
    ImGui::End();
}

void AssetsExplorer::LoadFileTexture(const std::string& path)
{
    try 
    {
        Texture newTexture;
        newTexture.LoadTexture(path, TextureFormat::RGBA8UnormSrgb, 128, 128);
        AssetsTextures[path] = newTexture;
    }
    catch (...)
    {
        Logger::Error("Failed to load image from this path: " + path);
    }
}

void AssetsExplorer::LoadFileTextures()
{
#ifdef RESOURCE_DIR
    fs::path root = fs::path(RESOURCE_DIR);
#else
    fs::path root = fs::current_path();
#endif

    if (!fs::exists(root) || !fs::is_directory(root)) {
        Logger::Error("RESOURCE_DIR not found or not a directory: " + root.string());
        return;
    }

    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec) {
            Logger::Error("Iterator error: " + ec.message());
            continue;
        }

        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) continue;

        fs::path p = entry.path();
        if (!IsImageExtension(p.extension().string())) continue;

        // make relative to RESOURCE_DIR
        std::error_code rel_ec;
        fs::path rel = fs::relative(p, root, rel_ec);
        if (rel_ec) {
            Logger::Error("Could not make relative path for: " + p.string());
            continue;
        }

        // turn into "/subdir/file.png"
        std::string localPath = "/" + rel.generic_string();

        try {
            LoadFileTexture(localPath); // your existing function
        }
        catch (...) {
            Logger::Error("Failed to load image from: " + localPath);
        }
    }
}


void AssetsExplorer::DrawAssetsWindow()
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
    for (const fs::path& part : rel) {
        if (part.empty() || part == ".") continue;
        ImGui::SameLine(); ImGui::TextUnformatted("\uf054"); ImGui::SameLine();
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
            bool hovered  = ImGui::IsItemHovered();
            bool dblClick = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            if (clicked) {
                selectedPath = en.p.string();
                selected = true;
            }            if (!en.isDir) {
                std::string extLower = en.p.extension().string();
                std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
                if ((extLower == ".glb" || extLower == ".gltf") && ImGui::BeginDragDropSource()) {
                    const std::string dragPath = en.p.string();
                    ImGui::SetDragDropPayload("MARIANA_ASSET_GLTF", dragPath.c_str(), dragPath.size() + 1);

                    const float previewW = 220.0f;
                    const ImVec2 p0 = ImGui::GetCursorScreenPos();
                    const ImVec2 p1 = ImVec2(p0.x + previewW, p0.y + 44.0f);
                    ImDrawList* previewDl = ImGui::GetWindowDrawList();
                    previewDl->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_Header, 0.55f), 6.0f);
                    previewDl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_HeaderActive), 6.0f, 0, 2.0f);
                    ImGui::Dummy(ImVec2(previewW, 44.0f));
                    ImGui::SetCursorScreenPos(ImVec2(p0.x + 10.0f, p0.y + 7.0f));
                    ImGui::TextUnformatted("Spawn prefab");
                    ImGui::SetCursorScreenPos(ImVec2(p0.x + 10.0f, p0.y + 24.0f));
                    ImGui::TextDisabled("%s", en.p.filename().string().c_str());
                    ImGui::EndDragDropSource();
                }
            }
            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();

            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (selected) {
                dl->AddRectFilled(rMin, rMax, ImGui::GetColorU32(ImGuiCol_Header, 0.28f), 6.0f);
                dl->AddRect(rMin, rMax, ImGui::GetColorU32(ImGuiCol_HeaderActive), 6.0f, 0, 2.0f);
            } else {
                if (hovered) {
                    dl->AddRectFilled(rMin, rMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.12f), 6.0f);
                }
                dl->AddRect(rMin, rMax, ImGui::GetColorU32(ImGuiCol_Border), 6.0f);
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
            else
            {
                // --- file: attempt to draw texture thumbnail ---
                try {
                    // Compute local path relative to RESOURCE_DIR in form "/sub/dir/file.ext"
                    std::error_code rel_ec;
                    fs::path rel = fs::relative(en.p, kRoot, rel_ec);
                    std::string localKey;
                    if (!rel_ec) {
                        localKey = "/" + rel.generic_string(); // forward slashes, leading slash
                    } else {
                        // fallback: use filename only (no leading dirs)
                        localKey = "/" + en.p.filename().generic_string();
                    }

                    // Look up in AssetsTextures (assumes EditorApp::AssetsTextures exists)
                    auto it = AssetsTextures.find(localKey);
                    if (it != AssetsTextures.end()) {
                        // Position cursor at thumbMin then draw texture of size tileSize x tileSize
                        ImGui::SetCursorScreenPos(thumbMin);

                        // IMPORTANT: gui.DrawTexture signature was given as gui.DrawTexture(texture, width, height)
                        // adapt this call if your API differs (e.g. needs pointer/reference)
                        m_gui.DrawTexture(it->second, (int)tileSize, (int)tileSize);

                        // after drawing, reset cursor to avoid interfering with label placement below
                        ImGui::SetCursorScreenPos(ImVec2(rMin.x, rMin.y + padding + tileSize + 0.0f));
                    } else {
                        // optional: draw a small file-type glyph or placeholder if texture missing
                        // Example: draw file-extension text faintly centered
                        std::string ext = en.p.has_extension() ? en.p.extension().string() : "";
                        if (!ext.empty()) {
                            ImVec2 extSz = ImGui::CalcTextSize(ext.c_str());
                            float x = thumbMin.x + ((thumbMax.x - thumbMin.x) - extSz.x) * 0.5f;
                            float y = thumbMin.y + ((thumbMax.y - thumbMin.y) - extSz.y) * 0.5f;
                            ImGui::SetCursorScreenPos(ImVec2(x, y));
                            ImGui::TextDisabled("%s", ext.c_str());
                        }
                    }
                } catch (...) {
                    // ignore any path errors, leave thumbnail empty
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




