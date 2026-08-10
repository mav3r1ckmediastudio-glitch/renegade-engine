#include <wiArchive.h>
#include <wiScene.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    bool WriteScene(const fs::path& path, const char* entityName)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;

        std::cout << "GATE4_SCENE_PHASE=construct_scene\n" << std::flush;
        wi::scene::Scene scene;
        scene.Entity_CreateTransform(entityName);

        std::cout << "GATE4_SCENE_PHASE=open_archive\n" << std::flush;
        wi::Archive archive(path.generic_u8string(), false, false);
        if (!archive.IsOpen())
            return false;

        std::cout << "GATE4_SCENE_PHASE=serialize_begin\n" << std::flush;
        scene.Serialize(archive);
        std::cout << "GATE4_SCENE_PHASE=serialize_end\n" << std::flush;

        std::cout << "GATE4_SCENE_PHASE=save_begin\n" << std::flush;
        const bool saved = archive.SaveFile(path.generic_u8string());
        std::cout << "GATE4_SCENE_PHASE=save_end saved="
                  << (saved ? "true" : "false") << '\n' << std::flush;

        archive = wi::Archive();
        return saved && fs::is_regular_file(path, ec) && !ec;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Gate 4 scene fixture expects one output directory.\n"
                  << std::flush;
        std::_Exit(2);
    }

    const fs::path output = fs::absolute(fs::u8path(argv[1]));
    const fs::path scene = output / "Gate4SelfContained.wiscene";
    if (!WriteScene(scene, "LP06 Gate 4 Self-Contained Scene"))
    {
        std::cerr << "Could not write Gate 4 self-contained WISCENE fixture.\n"
                  << std::flush;
        std::_Exit(1);
    }

    std::cout << "GATE4_SCENE_PHASE=complete path="
              << scene.generic_u8string() << '\n' << std::flush;

    // Wicked registers process-lifetime teardown callbacks as part of its
    // static runtime. This fixture has already closed and verified the only
    // file it owns; bypass those unrelated teardown callbacks so a successful
    // serializer proof cannot be stranded after main() has completed.
    std::_Exit(0);
}
