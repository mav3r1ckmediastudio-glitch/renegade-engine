#pragma once
#include <WickedEngine.h>
#include <memory>
#include <string>
namespace renegade::bridge
{
class StudioSession;
}
namespace renegade::studio
{
class DialogueEditorWindow final : public wi::gui::Window
{
  public:
    DialogueEditorWindow();
    ~DialogueEditorWindow();
    void Create();
    void Open(bridge::StudioSession &, wi::RenderPath2D &);
    void Tick(bridge::StudioSession &, wi::RenderPath2D &);
    bool TakeTestGameRequest();
    bool TakeDisplayEditorRequest(std::string& id,std::string& path);
    void Render(const wi::Canvas &, wi::graphics::CommandList) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace renegade::studio
