#include <wiArchive.h>
#include <wiScene.h>

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

        wi::scene::Scene scene;
        scene.Entity_CreateTransform(entityName);

        wi::Archive archive(path.generic_u8string(), false, false);
        if (!archive.IsOpen())
            return false;
        scene.Serialize(archive);
        const bool saved = archive.SaveFile(path.generic_u8string());
        archive = wi::Archive();
        return saved && fs::is_regular_file(path, ec) && !ec;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Gate 4 scene fixture expects one output directory.\n";
        return 2;
    }

    const fs::path output = fs::absolute(fs::u8path(argv[1]));
    const fs::path scene = output / "Gate4SelfContained.wiscene";
    if (!WriteScene(scene, "LP06 Gate 4 Self-Contained Scene"))
    {
        std::cerr << "Could not write Gate 4 self-contained WISCENE fixture.\n";
        return 1;
    }

    std::cout << scene.generic_u8string() << '\n';
    return 0;
}
