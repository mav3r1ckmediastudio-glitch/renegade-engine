#pragma once

#include <cstddef>
#include <string>

#include <WickedEngine.h>

namespace renegade::bridge
{
    class SceneService
    {
    public:
        void NewScene();
        bool LoadScene(const std::string& filePath);

        [[nodiscard]] wi::scene::Scene& GetScene() noexcept;
        [[nodiscard]] const wi::scene::Scene& GetScene() const noexcept;
        [[nodiscard]] std::size_t EntityCount() const;
        [[nodiscard]] const std::string& CurrentPath() const noexcept;
        [[nodiscard]] const std::string& LastError() const noexcept;

    private:
        wi::scene::Scene scene_;
        std::string currentPath_;
        std::string lastError_;
    };
}
