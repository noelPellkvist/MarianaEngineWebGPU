#include <Editor/Windows/AssetsExplorer.hpp>

#include <imgui.h>
#include <filesystem>
#include <Mariana/Foundation/Logger.hpp>
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

AssetsExplorer::AssetsExplorer(GUI& gui, std::filesystem::path root)
    : EditorWindow("Assets", gui, true), m_root(std::move(root))
{
    // Thumbnails are loaded lazily as their folder is shown, not all at startup.
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

bool AssetsExplorer::LoadFileTexture(const std::string& path)
{
    try
    {
        Texture newTexture;
        // UNORM on purpose: ImGui draws in sRGB space, so thumbnails must not be decoded to linear.
        newTexture.LoadTexture(path, TextureFormat::RGBA8Unorm, 128, 128);
        AssetsTextures[path] = newTexture;
        return true;
    }
    catch (...)
    {
        Logger::Error("Failed to load image from this path: " + path);
        return false;
    }
}

void AssetsExplorer::ScanDirectory(const fs::path& dir, const fs::path& root)
{
    m_entries.clear();
    m_entriesDir = dir;
    m_lastScan = ImGui::GetTime();

    std::error_code ec;
    m_entriesRelative = fs::relative(dir, root, ec);
    if (ec) m_entriesRelative.clear();

    try {
        for (auto &e : fs::directory_iterator(dir))
        {
            Entry entry;
            entry.path = e.path();
            entry.pathString = entry.path.string();
            entry.name = entry.path.filename().string();
            entry.isDir = e.is_directory();
            if (!entry.isDir)
            {
                entry.extension = entry.path.extension().string();
                std::transform(entry.extension.begin(), entry.extension.end(), entry.extension.begin(), ::tolower);
                entry.isImage = IsImageExtension(entry.extension);
                entry.isGltf = entry.extension == ".glb" || entry.extension == ".gltf";

                // Local key relative to the root in form "/sub/dir/file.ext" (thumbnail lookup)
                std::error_code rel_ec;
                fs::path rel = fs::relative(entry.path, root, rel_ec);
                entry.localKey = "/" + (rel_ec ? entry.path.filename() : rel).generic_string();
            }
            m_entries.push_back(std::move(entry));
        }
    } catch(...) {}

    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b){
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });
}

void AssetsExplorer::DrawAssetsWindow()
{
    const fs::path& kRoot = m_root;
    static float tileSize  = 96.0f;
    static float labelH    = 24.0f;
    static float padding   = 8.0f;

    static fs::path current = fs::exists(kRoot) ? fs::absolute(kRoot) : fs::current_path();
    static std::string selectedPath;

    // Re-list the folder when navigating, otherwise about once a second (not every frame).
    if (current != m_entriesDir || ImGui::GetTime() - m_lastScan > 1.0)
    {
        if (!fs::exists(current) || !fs::is_directory(current)) current = kRoot;
        ScanDirectory(current, kRoot);
    }

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

    fs::path accum = kRoot;
    for (const fs::path& part : m_entriesRelative) {
        if (part.empty() || part == ".") continue;
        ImGui::SameLine(); ImGui::TextUnformatted("\uf054"); ImGui::SameLine();
        std::string seg = part.string();
        if (ImGui::SmallButton(seg.c_str())) { accum /= part; current = accum; selectedPath.clear(); }
        else { accum /= part; }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::Separator();

    const std::vector<Entry>& items = m_entries;
    int thumbnailsLoaded = 0;

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

            bool selected = (!selectedPath.empty() && selectedPath == en.pathString);

            // Invisible button to capture clicks/double-clicks
            ImGui::InvisibleButton("tile", ImVec2(cellW, cellH));
            bool clicked  = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool hovered  = ImGui::IsItemHovered();
            bool dblClick = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            if (clicked) {
                selectedPath = en.pathString;
                selected = true;
            }            if (!en.isDir) {
                if (en.isGltf && ImGui::BeginDragDropSource()) {
                    const std::string& dragPath = en.pathString;
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
                    ImGui::TextDisabled("%s", en.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (en.isImage && ImGui::BeginDragDropSource()) {
                    const std::string& dragPath = en.pathString;
                    ImGui::SetDragDropPayload("MARIANA_ASSET_IMAGE", dragPath.c_str(), dragPath.size() + 1);
                    ImGui::TextUnformatted("Drop on a material texture slot");
                    ImGui::TextDisabled("%s", en.name.c_str());
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
                    const std::string& localKey = en.localKey;

                    // Load the thumbnail the first time it's shown, at most one per frame so
                    // opening a big folder doesn't stall.
                    if (en.isImage && thumbnailsLoaded == 0 &&
                        !AssetsTextures.contains(localKey) && !m_failedThumbnails.contains(localKey)) {
                        ++thumbnailsLoaded;
                        if (!LoadFileTexture(localKey))
                            m_failedThumbnails.insert(localKey);
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
                        const std::string& ext = en.extension;
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
            const std::string& name = en.name;
            ImVec2 textSz = ImGui::CalcTextSize(name.c_str(), nullptr, true, cellW - padding*2.0f);
            float textX = rMin.x + (cellW - textSz.x) * 0.5f;
            float textY = thumbMax.y + (labelH - textSz.y) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(textX, textY));
            ImGui::PushTextWrapPos(rMin.x + cellW - padding);
            ImGui::TextUnformatted(name.c_str());
            ImGui::PopTextWrapPos();

            // Click behavior
            if (clicked) selectedPath = en.pathString;
            if (dblClick) {
                if (en.isDir) { current = en.path; selectedPath.clear(); }
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




