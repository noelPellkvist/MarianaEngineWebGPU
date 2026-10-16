#pragma once
#include <Mariana/Foundation/FileWatcher.hpp>
#include <Mariana/Foundation/Image.hpp>
#include <Mariana/Foundation/TaskQueue.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Platform/IInput.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class ThumbnailRenderer;
struct ThumbnailSource;

// Thumbnails of a project's files for the Assets browser: images shrunk, models and .material files
// rendered by the engine (ThumbnailRenderer). A FileWatcher notices files being added and changed,
// and the files they are made from (a glTF's buffers and images, a material's images), and the
// thumbnails are remade in the background: reading, parsing and decoding on worker threads, the
// rendering on the main thread, one at a time. They are kept as PNGs in <project>/.mariana/
// thumbnails/ with index.json saying what each was made from (size and write time of the file and
// of every dependency), so after a restart only what changed is made again.
class ThumbnailCache
{
public:
    static constexpr uint32_t kThumbnailSize = 128; // stored
    static constexpr uint32_t kRenderSize = 256;    // rendered, then halved

    ThumbnailCache(std::filesystem::path projectRoot, IInput& input);
    ~ThumbnailCache();

    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;

    // Once per frame on the main thread, before the UI: takes in file changes and finished work,
    // starts more and renders at most one thumbnail.
    void Update();

    // The thumbnail of a project file by its resource path ("/Models/x.glb"), or nullptr while
    // there is none. Asking puts it first in line (natively the rest are made too, on the web only
    // what is asked for).
    Texture* Get(const std::string& resourcePath);
    // Whether it is being made or remade.
    bool IsPending(const std::string& resourcePath) const;

    // Images, models (.gltf, .glb) and .material files have thumbnails.
    static bool HasThumbnail(const std::filesystem::path& file);

private:
    enum class Kind { None, Image, Model, Material };

    struct FileState
    {
        uint64_t size = 0;
        int64_t modified = 0;
        bool operator==(const FileState&) const = default;
    };
    using Dependencies = std::vector<std::pair<std::string, FileState>>;

    struct Entry
    {
        Kind kind = Kind::None;
        bool exists = false; // seen by the watcher
        FileState state;     // as the watcher last saw it

        // The stored thumbnail (index.json): what it was made from.
        bool stored = false;
        bool failed = false; // nothing to show; not retried until something changes
        FileState madeFrom;
        Dependencies dependencies;

        std::unique_ptr<Texture> texture;

        bool stale = false;   // to be checked (and remade or loaded) once the file settles
        bool working = false; // a task for it is under way
        double changedAt = 0.0;
        uint64_t requestedFrame = 0; // 0 = never
        uint32_t generation = 0; // bumped on every change; results of older ones are dropped
    };

    // Worker -> main thread.
    struct Result
    {
        enum class Type { Loaded, Made, Prepared };
        Type type = Type::Loaded;
        std::string key;
        uint32_t generation = 0;
        bool heavy = false;                      // ends a model or material thumbnail
        Image::Pixels thumbnail;                 // Loaded, Made
        std::shared_ptr<ThumbnailSource> source; // Prepared
        FileState madeFrom;
        Dependencies dependencies;
        std::string error;
    };

    struct RenderJob
    {
        std::string key;
        uint32_t generation = 0;
        std::shared_ptr<ThumbnailSource> source;
        FileState madeFrom;
        Dependencies dependencies;
    };

    std::filesystem::path m_Root;
    std::filesystem::path m_Folder; // <root>/.mariana/thumbnails
    IInput& m_Input;
    bool m_GenerateAll;

    std::unordered_map<std::string, Entry> m_Entries;               // by key
    std::unordered_map<std::string, FileState> m_Files;             // every file the watcher saw
    std::unordered_map<std::string, std::unordered_set<std::string>> m_Dependents; // file -> entries made from it
    std::unordered_set<std::string> m_Stale;
    uint64_t m_Frame = 0;
    bool m_Scanned = false;
    bool m_IndexDirty = false;
    double m_LastIndexSave = 0.0;

    std::unique_ptr<ThumbnailRenderer> m_Renderer; // made on first use
    std::optional<RenderJob> m_Waiting;   // prepared, waiting for the renderer
    std::optional<RenderJob> m_Rendering; // waiting for the image to come back
    bool m_HeavyBusy = false;             // one model or material thumbnail at a time
    size_t m_LightTasks = 0;

    std::mutex m_ResultsMutex;
    std::vector<Result> m_Results;

    // Declared last: destroyed first, so no task or scan outlives what it uses.
    FileWatcher m_Watcher;
    TaskQueue m_LightWorker; // images, PNGs
    TaskQueue m_HeavyWorker; // parsing models and materials

    // "/sub/file.ext" for files in the project, "file:<absolute path>" for others.
    std::string KeyOf(const std::filesystem::path& file) const;
    std::filesystem::path PathOf(const std::string& key) const;
    FileState CurrentState(const std::string& key) const;
    static FileState Stat(const std::filesystem::path& file);
    static Kind KindOf(const std::string& path);

    void OnFileEvent(const FileWatcher::Event& event, double changedAt);
    void MarkStale(const std::string& key, Entry& entry, double changedAt);
    void RemoveEntry(const std::string& key);
    void SetDependencies(const std::string& key, Entry& entry, Dependencies dependencies);
    // Asked for (Get) in the last couple of frames.
    bool Requested(const Entry& entry) const;
    // Its stored thumbnail was made from what is there now.
    bool IsFresh(const std::string& key, const Entry& entry) const;

    void Schedule(double now);
    bool Start(const std::string& key, Entry& entry);
    void Post(Result result);
    void Apply(Result& result);
    // A thumbnail was made (empty: there is none to make); `saved` = its PNG was written.
    void Record(const std::string& key, Entry& entry, const FileState& madeFrom, Dependencies dependencies, const Image::Pixels& thumbnail, bool saved);
    void UpdateRendering();

    void LoadIndex();
    void SaveIndex();
    // After the first scan: forgets thumbnails of files that are gone, deletes stray PNGs.
    void DropMissing();
};
