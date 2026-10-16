#include <Editor/ThumbnailCache.hpp>
#include <Editor/ThumbnailRenderer.hpp>

#include <Mariana/Foundation/Logger.hpp>

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
    // Bump to remake every stored thumbnail (when the way they look changes).
    constexpr int kIndexVersion = 1;
    // A changed file is left alone this long, so files still being written (copied, exported)
    // are read once they are complete.
    constexpr double kSettleSeconds = 0.5;
    constexpr size_t kMaxLightTasks = 8;
    constexpr const char* kFilePrefix = "file:";

    double Now()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::string LowerExtension(const std::string& path)
    {
        std::string extension = fs::path(path).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return extension;
    }

    // The PNG a key's thumbnail is stored as (FNV-1a of the key).
    std::string ImageName(const std::string& key)
    {
        uint64_t hash = 1469598103934665603ull;
        for (unsigned char c : key)
            hash = (hash ^ c) * 1099511628211ull;
        char name[24];
        std::snprintf(name, sizeof(name), "%016llx.png", static_cast<unsigned long long>(hash));
        return name;
    }

    std::unique_ptr<Texture> MakeTexture(const Image::Pixels& image)
    {
        // UNORM on purpose: ImGui draws in sRGB space, so thumbnails must not be decoded to linear.
        auto texture = std::make_unique<Texture>();
        texture->CreateTexture(image.width, image.height, TextureFormat::RGBA8Unorm);
        texture->UploadTexture(image.rgba.data(), image.rgba.size(), image.width, image.height);
        return texture;
    }
}

ThumbnailCache::ThumbnailCache(fs::path projectRoot, IInput& input)
    : m_Root(fs::absolute(projectRoot).lexically_normal()),
      m_Folder(m_Root / ".mariana" / "thumbnails"),
      m_Input(input),
#if defined(__EMSCRIPTEN__)
      // No threads: everything would be made on the main thread, so only what is looked at is.
      m_GenerateAll(false),
#else
      m_GenerateAll(true),
#endif
      m_Watcher(m_Root)
{
    LoadIndex();
}

ThumbnailCache::~ThumbnailCache()
{
    if (m_IndexDirty)
        SaveIndex();
}

bool ThumbnailCache::HasThumbnail(const fs::path& file)
{
    return KindOf(file.string()) != Kind::None;
}

ThumbnailCache::Kind ThumbnailCache::KindOf(const std::string& path)
{
    static const char* const images[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".psd", ".hdr", ".pic", ".ppm" };
    const std::string extension = LowerExtension(path);
    if (extension == ".gltf" || extension == ".glb")
        return Kind::Model;
    if (extension == ".material")
        return Kind::Material;
    for (const char* image : images)
        if (extension == image)
            return Kind::Image;
    return Kind::None;
}

std::string ThumbnailCache::KeyOf(const fs::path& file) const
{
    const fs::path normal = file.lexically_normal();
    const fs::path relative = normal.lexically_relative(m_Root);
    if (relative.empty() || *relative.begin() == "..")
        return kFilePrefix + normal.generic_string();
    return "/" + relative.generic_string();
}

fs::path ThumbnailCache::PathOf(const std::string& key) const
{
    if (key.rfind(kFilePrefix, 0) == 0)
        return fs::path(key.substr(std::char_traits<char>::length(kFilePrefix)));
    return m_Root / fs::path(key.substr(1));
}

ThumbnailCache::FileState ThumbnailCache::CurrentState(const std::string& key) const
{
    if (key.rfind(kFilePrefix, 0) != 0)
    {
        const auto it = m_Files.find(key);
        return it == m_Files.end() ? FileState{} : it->second;
    }
    // Outside the project (unwatched): asked directly.
    return Stat(PathOf(key));
}

ThumbnailCache::FileState ThumbnailCache::Stat(const fs::path& file)
{
    FileState state;
    FileWatcher::Stat(file, state.size, state.modified);
    return state;
}

void ThumbnailCache::Update()
{
    ++m_Frame;
    const double now = Now();

    // The first scan lists what was there at startup: nothing to wait for.
    const bool firstScan = !m_Watcher.Scanned();
    m_Watcher.Poll([&](const FileWatcher::Event& event) { OnFileEvent(event, firstScan ? -1e9 : now); });
    if (!m_Scanned && m_Watcher.Scanned())
    {
        m_Scanned = true;
        DropMissing();
    }

    // Web: the workers' tasks run here, a little per frame.
    m_LightWorker.Pump(std::chrono::milliseconds(4));
    m_HeavyWorker.Pump(std::chrono::milliseconds(8));

    std::vector<Result> results;
    {
        std::lock_guard<std::mutex> lock(m_ResultsMutex);
        results.swap(m_Results);
    }
    for (Result& result : results)
        Apply(result);

    UpdateRendering();
    if (m_Scanned)
        Schedule(now);

    if (m_IndexDirty && now - m_LastIndexSave > 1.0)
    {
        SaveIndex();
        m_LastIndexSave = now;
    }
}

Texture* ThumbnailCache::Get(const std::string& resourcePath)
{
    const auto it = m_Entries.find(resourcePath);
    if (it == m_Entries.end())
        return nullptr;
    it->second.requestedFrame = m_Frame;
    return it->second.texture.get();
}

bool ThumbnailCache::IsPending(const std::string& resourcePath) const
{
    const auto it = m_Entries.find(resourcePath);
    return it != m_Entries.end() && it->second.exists && (it->second.stale || it->second.working);
}

void ThumbnailCache::OnFileEvent(const FileWatcher::Event& event, double changedAt)
{
    const std::string key = KeyOf(event.file);
    const bool removed = event.change == FileWatcher::Change::Removed;
    if (removed)
        m_Files.erase(key);
    else
        m_Files[key] = { event.size, event.modified };

    // Thumbnails made from this file are out of date.
    if (const auto dependents = m_Dependents.find(key); dependents != m_Dependents.end())
    {
        for (const std::string& dependent : dependents->second)
        {
            const auto entry = m_Entries.find(dependent);
            if (entry != m_Entries.end() && entry->second.exists)
                MarkStale(dependent, entry->second, changedAt);
        }
    }

    const Kind kind = KindOf(key);
    if (kind == Kind::None)
        return;
    if (removed)
    {
        RemoveEntry(key);
        return;
    }
    Entry& entry = m_Entries[key];
    entry.kind = kind;
    entry.exists = true;
    entry.state = { event.size, event.modified };
    MarkStale(key, entry, changedAt);
}

void ThumbnailCache::MarkStale(const std::string& key, Entry& entry, double changedAt)
{
    entry.stale = true;
    entry.changedAt = std::max(entry.changedAt, changedAt);
    ++entry.generation;
    m_Stale.insert(key);
}

void ThumbnailCache::RemoveEntry(const std::string& key)
{
    const auto it = m_Entries.find(key);
    if (it == m_Entries.end())
        return;
    if (it->second.stored)
    {
        std::error_code ec;
        fs::remove(m_Folder / ImageName(key), ec);
        m_IndexDirty = true;
    }
    SetDependencies(key, it->second, {});
    m_Stale.erase(key);
    m_Entries.erase(it);
}

void ThumbnailCache::SetDependencies(const std::string& key, Entry& entry, Dependencies dependencies)
{
    for (const auto& [dependency, state] : entry.dependencies)
    {
        const auto it = m_Dependents.find(dependency);
        if (it == m_Dependents.end())
            continue;
        it->second.erase(key);
        if (it->second.empty())
            m_Dependents.erase(it);
    }
    entry.dependencies = std::move(dependencies);
    for (const auto& [dependency, state] : entry.dependencies)
        m_Dependents[dependency].insert(key);
}

bool ThumbnailCache::Requested(const Entry& entry) const
{
    return entry.requestedFrame != 0 && entry.requestedFrame + 2 >= m_Frame;
}

bool ThumbnailCache::IsFresh(const std::string& key, const Entry& entry) const
{
    if (!entry.stored || !(entry.madeFrom == entry.state))
        return false;
    for (const auto& [dependency, state] : entry.dependencies)
        if (!(CurrentState(dependency) == state))
            return false;
    std::error_code ec;
    return entry.failed || fs::exists(m_Folder / ImageName(key), ec);
}

void ThumbnailCache::Schedule(double now)
{
    // What was asked for lately (shown in the Assets browser) goes first.
    std::vector<std::string> candidates;
    std::vector<std::string> done;
    for (const std::string& key : m_Stale)
    {
        const auto it = m_Entries.find(key);
        if (it == m_Entries.end() || !it->second.exists)
        {
            done.push_back(key);
            continue;
        }
        const Entry& entry = it->second;
        if (entry.working || now - entry.changedAt < kSettleSeconds || (!Requested(entry) && !m_GenerateAll))
            continue;
        candidates.push_back(key);
    }
    for (const std::string& key : done)
        m_Stale.erase(key);

    std::stable_partition(candidates.begin(), candidates.end(), [this](const std::string& key) {
        return Requested(m_Entries[key]);
    });
    for (const std::string& key : candidates)
    {
        Entry& entry = m_Entries[key];
        if (Start(key, entry))
        {
            entry.stale = false;
            m_Stale.erase(key);
        }
        if (m_LightTasks >= kMaxLightTasks && m_HeavyBusy)
            break;
    }
}

bool ThumbnailCache::Start(const std::string& key, Entry& entry)
{
    const uint32_t generation = entry.generation;
    const fs::path file = PathOf(key);

    if (IsFresh(key, entry))
    {
        if (entry.texture || entry.failed)
            return true;
        // Made before: shown from its PNG.
        if (m_LightTasks >= kMaxLightTasks)
            return false;
        ++m_LightTasks;
        entry.working = true;
        m_LightWorker.Enqueue([this, key, generation, png = m_Folder / ImageName(key)]() {
            Result result;
            result.type = Result::Type::Loaded;
            result.key = key;
            result.generation = generation;
            Image::Load(png, result.thumbnail, result.error);
            Post(std::move(result));
        });
        return true;
    }

    if (entry.kind == Kind::Image)
    {
        if (m_LightTasks >= kMaxLightTasks)
            return false;
        ++m_LightTasks;
        entry.working = true;
        m_LightWorker.Enqueue([this, key, generation, file, png = m_Folder / ImageName(key)]() {
            Result result;
            result.type = Result::Type::Made;
            result.key = key;
            result.generation = generation;
            result.madeFrom = Stat(file);
            Image::Pixels image;
            if (Image::Load(file, image, result.error))
            {
                result.thumbnail = Image::Downscale(std::move(image), kThumbnailSize);
                Image::SavePNG(png, result.thumbnail, result.error);
            }
            Post(std::move(result));
        });
        return true;
    }

    // Models and materials: read and decoded here, rendered on the main thread (UpdateRendering).
    if (m_HeavyBusy)
        return false;
    m_HeavyBusy = true;
    entry.working = true;
    const ThumbnailSource::Kind kind = entry.kind == Kind::Model ? ThumbnailSource::Kind::Model : ThumbnailSource::Kind::Material;
    m_HeavyWorker.Enqueue([this, key, generation, file, kind]() {
        Result result;
        result.type = Result::Type::Prepared;
        result.key = key;
        result.generation = generation;
        result.heavy = true;
        result.madeFrom = Stat(file);
        result.source = std::make_shared<ThumbnailSource>(ThumbnailSource::Prepare(kind, file, kRenderSize));
        for (const fs::path& dependency : result.source->dependencies)
            result.dependencies.emplace_back(KeyOf(dependency), Stat(dependency));
        Post(std::move(result));
    });
    return true;
}

void ThumbnailCache::Post(Result result)
{
    std::lock_guard<std::mutex> lock(m_ResultsMutex);
    m_Results.push_back(std::move(result));
}

void ThumbnailCache::Apply(Result& result)
{
    if (result.type != Result::Type::Prepared)
        --m_LightTasks;

    const auto it = m_Entries.find(result.key);
    const bool current = it != m_Entries.end() && it->second.generation == result.generation;
    if (it != m_Entries.end() && (result.type != Result::Type::Prepared || !current))
        it->second.working = false;
    if (!current)
    {
        // Changed (or gone) meanwhile: it is made again from the new version.
        if (result.heavy)
            m_HeavyBusy = false;
        return;
    }
    Entry& entry = it->second;

    switch (result.type)
    {
        case Result::Type::Loaded:
            if (result.thumbnail.Empty())
            {
                // The stored PNG is unreadable: make it again.
                entry.stored = false;
                MarkStale(result.key, entry, -1e9);
            }
            else
            {
                entry.texture = MakeTexture(result.thumbnail);
            }
            break;

        case Result::Type::Made:
            if (result.heavy)
                m_HeavyBusy = false;
            if (!result.error.empty())
                Logger::Warning("Thumbnail of " + result.key + ": " + result.error);
            // An error with an image means it was not saved: shown, but made again next time.
            Record(result.key, entry, result.madeFrom, std::move(result.dependencies), result.thumbnail, result.error.empty());
            break;

        case Result::Type::Prepared:
            m_Waiting = RenderJob{ result.key, result.generation, std::move(result.source), result.madeFrom, std::move(result.dependencies) };
            break;
    }
}

void ThumbnailCache::Record(const std::string& key, Entry& entry, const FileState& madeFrom, Dependencies dependencies, const Image::Pixels& thumbnail, bool saved)
{
    entry.failed = thumbnail.Empty();
    entry.stored = entry.failed || saved;
    entry.madeFrom = madeFrom;
    SetDependencies(key, entry, std::move(dependencies));
    if (entry.failed)
        entry.texture.reset();
    else
        entry.texture = MakeTexture(thumbnail);
    m_IndexDirty = true;
    // Something changed while it was made (the states are what was read).
    if (entry.stored && !IsFresh(key, entry))
        MarkStale(key, entry, Now());
}

void ThumbnailCache::UpdateRendering()
{
    if (m_Rendering)
    {
        Image::Pixels image;
        if (!m_Renderer->TryGetResult(image))
            return;
        RenderJob job = std::move(*m_Rendering);
        m_Rendering.reset();

        // Halved and saved on the worker; Apply shows it.
        ++m_LightTasks;
        m_LightWorker.Enqueue([this, job = std::move(job), image = std::move(image)]() mutable {
            Result result;
            result.type = Result::Type::Made;
            result.key = job.key;
            result.generation = job.generation;
            result.heavy = true;
            result.madeFrom = job.madeFrom;
            result.dependencies = std::move(job.dependencies);
            result.thumbnail = Image::Downscale(std::move(image), kThumbnailSize);
            Image::SavePNG(m_Folder / ImageName(job.key), result.thumbnail, result.error);
            Post(std::move(result));
        });
    }

    if (!m_Waiting)
        return;
    RenderJob job = std::move(*m_Waiting);
    m_Waiting.reset();

    const auto it = m_Entries.find(job.key);
    if (it == m_Entries.end() || it->second.generation != job.generation)
    {
        if (it != m_Entries.end())
            it->second.working = false;
        m_HeavyBusy = false;
        return;
    }

    if (!m_Renderer)
        m_Renderer = std::make_unique<ThumbnailRenderer>(m_Input, kRenderSize);
    std::string error;
    if (m_Renderer->Render(*job.source, error))
    {
        job.source.reset();
        m_Rendering = std::move(job);
        return;
    }

    Logger::Warning("No thumbnail for " + job.key + ": " + error);
    it->second.working = false;
    m_HeavyBusy = false;
    Record(job.key, it->second, job.madeFrom, std::move(job.dependencies), {}, false);
}

void ThumbnailCache::LoadIndex()
{
    std::ifstream stream(m_Folder / "index.json", std::ios::binary);
    if (!stream)
        return;
    try
    {
        std::ostringstream text;
        text << stream.rdbuf();
        const nlohmann::json json = nlohmann::json::parse(text.str());
        // Made differently before: everything is made again (the PNGs are overwritten).
        if (json.value("version", 0) != kIndexVersion || json.value("size", 0u) != kThumbnailSize)
            return;
        const auto thumbnails = json.find("thumbnails");
        if (thumbnails == json.end() || !thumbnails->is_object())
            return;
        for (const auto& [key, item] : thumbnails->items())
        {
            const Kind kind = KindOf(key);
            if (kind == Kind::None)
                continue;
            Entry& entry = m_Entries[key];
            entry.kind = kind;
            entry.stored = true;
            entry.failed = item.value("failed", false);
            entry.madeFrom = { item.value("size", uint64_t{ 0 }), item.value("modified", int64_t{ 0 }) };
            Dependencies dependencies;
            if (const auto list = item.find("dependencies"); list != item.end() && list->is_object())
                for (const auto& [dependency, state] : list->items())
                    if (state.is_array() && state.size() == 2)
                        dependencies.emplace_back(dependency, FileState{ state[0].get<uint64_t>(), state[1].get<int64_t>() });
            SetDependencies(key, entry, std::move(dependencies));
        }
    }
    catch (const std::exception& e)
    {
        Logger::Warning("Ignoring the thumbnail index: " + std::string(e.what()));
    }
}

void ThumbnailCache::SaveIndex()
{
    nlohmann::json json;
    json["version"] = kIndexVersion;
    json["size"] = kThumbnailSize;
    nlohmann::json& thumbnails = json["thumbnails"] = nlohmann::json::object();
    for (const auto& [key, entry] : m_Entries)
    {
        if (!entry.stored)
            continue;
        nlohmann::json item;
        item["size"] = entry.madeFrom.size;
        item["modified"] = entry.madeFrom.modified;
        if (entry.failed)
            item["failed"] = true;
        nlohmann::json dependencies = nlohmann::json::object();
        for (const auto& [dependency, state] : entry.dependencies)
            dependencies[dependency] = { state.size, state.modified };
        item["dependencies"] = std::move(dependencies);
        thumbnails[key] = std::move(item);
    }

    std::error_code ec;
    fs::create_directories(m_Folder, ec);
    std::ofstream stream(m_Folder / "index.json", std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        Logger::Warning("Could not write " + (m_Folder / "index.json").string());
        return;
    }
    stream << json.dump(2);
    m_IndexDirty = false;
}

void ThumbnailCache::DropMissing()
{
    std::vector<std::string> missing;
    for (const auto& [key, entry] : m_Entries)
        if (!entry.exists)
            missing.push_back(key);
    for (const std::string& key : missing)
        RemoveEntry(key);

    // PNGs no thumbnail uses any more (their files went while the editor was closed).
    std::unordered_set<std::string> used;
    for (const auto& [key, entry] : m_Entries)
        used.insert(ImageName(key));
    std::vector<fs::path> stray;
    std::error_code ec;
    for (const fs::directory_entry& file : fs::directory_iterator(m_Folder, ec))
        if (file.path().extension() == ".png" && !used.count(file.path().filename().string()))
            stray.push_back(file.path());
    for (const fs::path& file : stray)
        fs::remove(file, ec);
}
