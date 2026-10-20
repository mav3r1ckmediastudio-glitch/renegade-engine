#include "RenegadeAssistantDock.h"

#include <algorithm>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#include "json.hpp"
#include <cctype>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace renegade::studio
{
namespace
{
    constexpr std::size_t MaximumChatEntries = 80;
    constexpr std::size_t MaximumEntryLength = 1200;
    constexpr std::size_t CardsPerPage = 3;
    const std::filesystem::path HistoryFile =
        "Saved/Assistant/StudioChatHistory.txt";
    const std::filesystem::path GuideFile =
        "Saved/Assistant/RetargetGuideStep.txt";

    std::string Clean(std::string input, const std::size_t limit)
    {
        for (char& character : input)
        {
            if (character == '\n' || character == '\r' || character == '\t')
                character = ' ';
        }
        input.erase(
            std::remove_if(input.begin(), input.end(),
                [](unsigned char c) { return c < 32 && c != ' '; }),
            input.end());
        if (input.size() > limit)
            input.resize(limit);
        return input;
    }

    void ConfigureLabel(
        wi::gui::Label& label,
        wi::gui::Window& owner,
        const char* name,
        const int fontSize,
        const wi::Color colour)
    {
        label.Create(name);
        label.font.params.size = fontSize;
        label.font.params.color = colour;
        label.font.params.h_align = wi::font::WIFALIGN_LEFT;
        label.SetColor(wi::Color::Transparent());
        owner.AddWidget(&label);
    }

    void ConfigureButton(
        RenegadeButton& button,
        wi::gui::Window& owner,
        const char* name,
        const char* label,
        const std::function<void()>& clicked)
    {
        button.Create(name);
        button.SetText(label);
        button.SetRenderTextSize(11);
        button.OnClick([clicked](const wi::gui::EventArgs&)
        {
            if (clicked)
                clicked();
        });
        owner.AddWidget(&button);
    }
}

void RenegadeAssistantPortrait::Update(const wi::Canvas& canvas, float dt)
{
    Widget::Update(canvas, dt);
    if (dt > 0.0f && dt < 0.5f)
        phase_ = std::fmod(phase_ + dt, 6.2831853f);
}

void RenegadeAssistantPortrait::Render(
    const wi::Canvas& canvas, const wi::graphics::CommandList cmd) const
{
    if (!IsVisible())
        return;
    ApplyScissor(canvas, scissorRect, cmd);
    const float x = translation.x;
    const float y = translation.y;
    const float w = scale.x;
    const float h = scale.y;
    const float pulse = 0.75f + std::sin(phase_ * 1.6f) * 0.16f;
    const auto glow = wi::Color(56, 176, 207,
        static_cast<std::uint8_t>(200.0f * pulse));
    const auto dim = wi::Color(32, 91, 116, 185);
    const auto eye = wi::Color(160, 236, 245, 240);
    const auto surface = wi::Color(8, 32, 44, 210);
    auto rect = [cmd](float a, float b, float c, float e, wi::Color color)
    {
        if (c <= 0.0f || e <= 0.0f) return;
        wi::image::Params params(a, b, c, e, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    };
    rect(x + 2, y + 4, w - 4, h - 8, surface);
    // Irregular luminous outer frame reads as a projection, not an animal.
    rect(x + 3, y + 4, 15, 2, glow);
    rect(x + 3, y + 4, 2, 14, glow);
    rect(x + w - 19, y + 4, 15, 2, glow);
    rect(x + w - 5, y + 4, 2, 14, glow);
    rect(x + 3, y + h - 7, 15, 2, glow);
    rect(x + 3, y + h - 19, 2, 14, glow);
    rect(x + w - 19, y + h - 7, 15, 2, glow);
    rect(x + w - 5, y + h - 19, 2, 14, glow);
    const float cx = x + w * 0.5f;
    // Fragmented forehead, temples and angular jaw.
    rect(cx - 19, y + 13, 38, 3, glow);
    rect(cx - 22, y + 17, 2, 23, dim);
    rect(cx + 20, y + 17, 2, 23, dim);
    rect(cx - 20, y + 41, 4, 9, glow);
    rect(cx + 16, y + 41, 4, 9, glow);
    rect(cx - 16, y + 51, 32, 2, dim);
    rect(cx - 7, y + 55, 14, 2, glow);
    // Paired bright eyes and a restrained mouth.
    rect(cx - 15, y + 30, 10, 3, eye);
    rect(cx + 5, y + 30, 10, 3, eye);
    rect(cx - 3, y + 39, 6, 2, dim);
    rect(cx - 8, y + 46, 16, 2, glow);
    // Projected data slices pulse lightly without demanding focus.
    for (int row = 0; row < 5; ++row)
    {
        const float offset = float(row) * 10.0f;
        rect(x + 8, y + 15 + offset, 3, 1, dim);
        rect(x + w - 11, y + 20 + offset, 3, 1, dim);
    }
}

void RenegadeAssistantDock::Create()
{
    panel_.Create("Renegade Assistant // Studio Companion",
        wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
    panel_.SetShadowRadius(0.0f);
    panel_.SetColor(wi::Color(12, 20, 28, 255));

    // Native, lightweight and deliberately original digital manifestation.
    portrait_.SetName("Renegade Assistant Digital Manifestation");
    panel_.AddWidget(&portrait_);
    ConfigureLabel(title_, panel_, "Assistant Title", 16,
        wi::Color(244, 248, 252, 255));
    title_.SetText("RENEGADE");
    ConfigureLabel(subtitle_, panel_, "Assistant Identity", 10,
        wi::Color(136, 193, 209, 255));
    subtitle_.SetText("STUDIO COMPANION  //  EARLY PREVIEW");
    ConfigureLabel(bubble_, panel_, "Assistant Speech Bubble", 11,
        wi::Color(218, 231, 240, 255));
    ConfigureLabel(guideTitle_, panel_, "Assistant Guide Title", 12,
        wi::Color(230, 235, 239, 255));
    ConfigureLabel(guideBody_, panel_, "Assistant Guide Step", 11,
        wi::Color(214, 223, 230, 255));
    ConfigureLabel(guideEvidence_, panel_, "Assistant Source Evidence", 9,
        wi::Color(141, 178, 194, 255));
    ConfigureLabel(historyTitle_, panel_, "Assistant Chat History Heading", 12,
        wi::Color(230, 236, 242, 255));
    for (std::size_t i = 0; i < historyCards_.size(); ++i)
    {
        const std::string name = "Assistant Chat Card " + std::to_string(i);
        ConfigureLabel(historyCards_[i], panel_, name.c_str(), 11,
            wi::Color(216, 229, 236, 255));
    }

    input_.Create("Assistant Chat Input");
    input_.SetPlaceholder("Ask for retargeting help...");
    input_.SetValue("");
    input_.SetCancelInputEnabled(false);
    input_.OnInputAccepted([this](const wi::gui::EventArgs& args)
    {
        SubmitText(args.sValue);
        input_.SetValue("");
    });
    panel_.AddWidget(&input_);

    ConfigureButton(send_, panel_, "Assistant Send", "SEND",
        [this]() { SubmitInput(); });
    ConfigureButton(close_, panel_, "Assistant Close", "HIDE",
        [this]() { SetOpen(false); });
    ConfigureButton(guideStart_, panel_, "Assistant Retarget Guide", "RETARGET GUIDE",
        [this]() { StartRetargetGuide(); });
    ConfigureButton(guidePrevious_, panel_, "Assistant Previous Guide Step", "BACK",
        [this]() { PreviousGuideStep(); });
    ConfigureButton(guideNext_, panel_, "Assistant Next Guide Step", "NEXT STEP",
        [this]() { NextGuideStep(); });
    ConfigureButton(older_, panel_, "Assistant Older Chat Entries", "OLDER",
        [this]()
        {
            historyFirst_ = historyFirst_ > CardsPerPage
                ? historyFirst_ - CardsPerPage : 0;
            RefreshVisibleHistory();
        });
    ConfigureButton(newer_, panel_, "Assistant Newer Chat Entries", "NEWER",
        [this]()
        {
            historyFirst_ = std::min(history_.size(),
                historyFirst_ + CardsPerPage);
            RefreshVisibleHistory();
        });

    LoadLocalState();
    if (history_.empty())
    {
        AddEntry(false,
            "Alright! I'm Renegade, your Studio companion. "
            "The retarget guide is ready. I can also chat through the "
            "optional local Qwen test server. No editor actions are connected.");
    }
    RefreshGuide();
    RefreshVisibleHistory();
    SetLayout(0.0f, 64.0f, layoutWidth_, layoutHeight_);
    SetOpen(false);
}

void RenegadeAssistantDock::SetLayout(
    const float x, const float y, const float width, const float height)
{
    layoutWidth_ = std::max(265.0f, width);
    layoutHeight_ = std::max(360.0f, height);
    panel_.SetPos(XMFLOAT2(x, y));
    panel_.SetSize(XMFLOAT2(layoutWidth_, layoutHeight_));
    const float contentWidth = layoutWidth_ - 24.0f;
    portrait_.SetPos(XMFLOAT2(12.0f, 10.0f));
    portrait_.SetSize(XMFLOAT2(80.0f, 78.0f));
    title_.SetPos(XMFLOAT2(98.0f, 10.0f));
    title_.SetSize(XMFLOAT2(contentWidth - 132.0f, 24.0f));
    subtitle_.SetPos(XMFLOAT2(98.0f, 38.0f));
    subtitle_.SetSize(XMFLOAT2(contentWidth - 84.0f, 44.0f));
    close_.SetPos(XMFLOAT2(layoutWidth_ - 67.0f, 6.0f));
    close_.SetSize(XMFLOAT2(55.0f, 25.0f));

    bubble_.SetPos(XMFLOAT2(12.0f, 90.0f));
    bubble_.SetSize(XMFLOAT2(contentWidth, 54.0f));
    guideStart_.SetPos(XMFLOAT2(12.0f, 149.0f));
    guideStart_.SetSize(XMFLOAT2(contentWidth, 31.0f));
    guideTitle_.SetPos(XMFLOAT2(12.0f, 189.0f));
    guideTitle_.SetSize(XMFLOAT2(contentWidth, 25.0f));
    guideBody_.SetPos(XMFLOAT2(12.0f, 218.0f));
    guideBody_.SetSize(XMFLOAT2(contentWidth, 125.0f));
    guideEvidence_.SetPos(XMFLOAT2(12.0f, 341.0f));
    guideEvidence_.SetSize(XMFLOAT2(contentWidth, 34.0f));
    const float half = (contentWidth - 6.0f) * 0.5f;
    guidePrevious_.SetPos(XMFLOAT2(12.0f, 382.0f));
    guidePrevious_.SetSize(XMFLOAT2(half, 28.0f));
    guideNext_.SetPos(XMFLOAT2(18.0f + half, 382.0f));
    guideNext_.SetSize(XMFLOAT2(half, 28.0f));

    const float inputTop = layoutHeight_ - 77.0f;
    const float historyTop = 427.0f;
    historyTitle_.SetPos(XMFLOAT2(12.0f, historyTop));
    historyTitle_.SetSize(XMFLOAT2(contentWidth, 24.0f));
    const float available = std::max(0.0f, inputTop - historyTop - 53.0f);
    const float rowHeight = std::min(94.0f, available / 3.0f);
    for (std::size_t i = 0; i < historyCards_.size(); ++i)
    {
        historyCards_[i].SetPos(XMFLOAT2(
            12.0f, historyTop + 27.0f + static_cast<float>(i) * rowHeight));
        historyCards_[i].SetSize(XMFLOAT2(contentWidth, rowHeight));
        historyCards_[i].SetVisible(rowHeight >= 40.0f);
    }
    older_.SetPos(XMFLOAT2(12.0f, inputTop - 31.0f));
    older_.SetSize(XMFLOAT2(half, 26.0f));
    newer_.SetPos(XMFLOAT2(18.0f + half, inputTop - 31.0f));
    newer_.SetSize(XMFLOAT2(half, 26.0f));
    input_.SetPos(XMFLOAT2(12.0f, inputTop));
    input_.SetSize(XMFLOAT2(std::max(110.0f, contentWidth - 74.0f), 29.0f));
    send_.SetPos(XMFLOAT2(layoutWidth_ - 72.0f, inputTop));
    send_.SetSize(XMFLOAT2(60.0f, 29.0f));
    RefreshVisibleHistory();
}

void RenegadeAssistantDock::SetOpen(const bool open)
{
    if (open_ == open)
    {
        panel_.SetVisible(open);
        return;
    }
    open_ = open;
    panel_.SetVisible(open);
    if (openChanged_)
        openChanged_(open);
}

void RenegadeAssistantDock::OnOpenChanged(std::function<void(bool)> changed)
{
    openChanged_ = std::move(changed);
}

std::string RenegadeAssistantDock::Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string RenegadeAssistantDock::Wrap(
    std::string_view text, const std::size_t maximumColumns,
    const std::size_t maximumLines)
{
    std::istringstream stream{std::string(text)};
    std::string result;
    std::string word;
    std::size_t column = 0, lines = 1;
    while (stream >> word)
    {
        if (word.size() > maximumColumns)
            word.resize(maximumColumns - 1);
        if (column != 0 && column + 1 + word.size() > maximumColumns)
        {
            if (lines == maximumLines)
            {
                result += "...";
                break;
            }
            result += '\n';
            ++lines;
            column = 0;
        }
        if (column != 0)
        {
            result += ' ';
            ++column;
        }
        result += word;
        column += word.size();
    }
    return result;
}

void RenegadeAssistantDock::SubmitInput()
{
    SubmitText(input_.GetText());
    input_.SetValue("");
}

void RenegadeAssistantDock::SubmitText(std::string text)
{
    text = Clean(std::move(text), MaximumEntryLength);
    if (text.find_first_not_of(' ') == std::string::npos)
        return;
    AddEntry(true, text);
    const std::string lower = Lower(text);
    if (lower.find("retarget") != std::string::npos ||
        lower.find("animation guide") != std::string::npos)
    {
        StartRetargetGuide();
    }
    else if (lower == "next" || lower == "next step")
    {
        NextGuideStep();
    }
    else if (lower == "back" || lower == "previous")
    {
        PreviousGuideStep();
    }
    else if ((lower.find("create") != std::string::npos ||
              lower.find("build") != std::string::npos ||
              lower.find("generate") != std::string::npos) &&
             (lower.find("level for me") != std::string::npos ||
              lower.find("game for me") != std::string::npos ||
              lower.find("world for me") != std::string::npos))
    {
        AddEntry(false,
            "I can't create an entire level or game for you. That's your "
            "creative work. I can guide you through building it, though.");
    }
    else if (lower == "alright" || lower == "alright?" ||
             lower == "hello" || lower == "hi" || lower == "hey")
    {
        AddEntry(false,
            "Alright! Good to see you. Want to try the retargeting guide?");
    }
    else
    {
        RequestLocalQwenReply();
    }
}

void RenegadeAssistantDock::Tick()
{
    if (!pendingReply_.valid())
        return;
    if (pendingReply_.wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready)
        return;
    try
    {
        AddEntry(false, pendingReply_.get());
    }
    catch (const std::exception&)
    {
        AddEntry(false,
            "The local Qwen test service is unavailable right now. "
            "The retarget guide still works without it.");
    }
}

void RenegadeAssistantDock::RequestLocalQwenReply()
{
    if (pendingReply_.valid())
    {
        if (pendingReply_.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready)
        {
            AddEntry(false,
                "I'm still thinking about your previous question. "
                "Give me a moment before sending another.");
            return;
        }
        Tick();
    }
    // All networking happens off the editor/update/render thread.
    std::vector<ChatEntry> snapshot = history_;
    pendingReply_ = std::async(std::launch::async,
        [history = std::move(snapshot)]() mutable
        {
            return AskLocalQwen(std::move(history));
        });
    bubbleText_ = "Thinking... You can keep using the editor.";
    RefreshBubble();
}

std::string RenegadeAssistantDock::AskLocalQwen(
    std::vector<ChatEntry> history)
{
    // Experimental loopback-only chat transport. NOT a general-purpose agent
    // or live Editor Bridge. No scene mutators or tool capabilities exposed.
    nlohmann::json data = {
        {"adapter", "v31"},
        {"messages", nlohmann::json::array()},
        {"max_tokens", 480}
    };
    const std::size_t first = history.size() > 14
        ? history.size() - 14 : 0;
    for (std::size_t i = first; i < history.size(); ++i)
        data["messages"].push_back({
            {"role", history[i].fromUser ? "user" : "assistant"},
            {"content", history[i].text}
        });
    const std::string payload = data.dump();

    auto closeHandle = [](HINTERNET handle)
    {
        if (handle != nullptr) WinHttpCloseHandle(handle);
    };
    const std::unique_ptr<std::remove_pointer_t<HINTERNET>,
        decltype(closeHandle)> session(
        WinHttpOpen(L"RenegadeStudioAssistant/0.1",
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
        closeHandle);
    if (!session)
        throw std::runtime_error("WinHTTP session failed");
    WinHttpSetTimeouts(session.get(), 1000, 2000, 2000, 45000);
    const std::unique_ptr<std::remove_pointer_t<HINTERNET>,
        decltype(closeHandle)> connection(
        // Knowledge-first loopback bridge on 18384; the experimental Qwen
        // chat service remains untouched on 8383 for ordinary conversation.
        WinHttpConnect(session.get(), L"127.0.0.1", 18384, 0),
        closeHandle);
    if (!connection)
        throw std::runtime_error("No local Qwen server");
    const std::unique_ptr<std::remove_pointer_t<HINTERNET>,
        decltype(closeHandle)> request(
        WinHttpOpenRequest(connection.get(), L"POST", L"/chat",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_REFRESH),
        closeHandle);
    if (!request)
        throw std::runtime_error("WinHTTP request failed");
    const wchar_t* headers = L"Content-Type: application/json\r\n";
    if (!WinHttpSendRequest(request.get(), headers, static_cast<DWORD>(-1),
            const_cast<char*>(payload.data()),
            static_cast<DWORD>(payload.size()),
            static_cast<DWORD>(payload.size()), 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
        throw std::runtime_error("Local Qwen service did not respond");
    DWORD status = 0;
    DWORD statusLength = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLength,
            WINHTTP_NO_HEADER_INDEX) || status != 200)
        throw std::runtime_error("Local Qwen returned an HTTP error");
    std::string response;
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available))
            throw std::runtime_error("Failed reading local Qwen response");
        if (available == 0) break;
        if (available > 128 * 1024 || response.size() + available > 128 * 1024)
            throw std::runtime_error("Unexpectedly large local Qwen response");
        const auto previous = response.size();
        response.resize(previous + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), response.data() + previous,
                available, &read))
            throw std::runtime_error("Failed reading Qwen response bytes");
        response.resize(previous + read);
        if (read == 0) break;
    }
    const auto parsed = nlohmann::json::parse(response);
    if (parsed.contains("error") && parsed["error"].is_string())
        throw std::runtime_error(parsed["error"].get<std::string>());
    if (!parsed.contains("answer") || !parsed["answer"].is_string())
        throw std::runtime_error("Local model response contained no answer");
    return parsed["answer"].get<std::string>();
}

void RenegadeAssistantDock::StartRetargetGuide()
{
    guideStep_ = 0;
    AddEntry(false,
        "Let's retarget those animations. I'll keep each step in the guide "
        "here while you use the editor.");
    RefreshGuide();
    SaveLocalState();
}

void RenegadeAssistantDock::NextGuideStep()
{
    if (guideStep_ < 0)
    {
        StartRetargetGuide();
        return;
    }
    if (guideStep_ >= static_cast<int>(RetargetSteps.size() - 1))
    {
        AddEntry(false,
            "That's the end of the guide. Check the resulting animation "
            "in the editor. I'll stay here if you need to revisit a step.");
        return;
    }
    ++guideStep_;
    AddEntry(false,
        "Step " + std::to_string(guideStep_ + 1) + ": " +
        RetargetSteps[guideStep_].heading);
    RefreshGuide();
    SaveLocalState();
}

void RenegadeAssistantDock::PreviousGuideStep()
{
    if (guideStep_ <= 0)
        return;
    --guideStep_;
    AddEntry(false,
        "Returning to step " + std::to_string(guideStep_ + 1) + ".");
    RefreshGuide();
    SaveLocalState();
}

void RenegadeAssistantDock::AddEntry(
    const bool fromUser, std::string text)
{
    history_.push_back(ChatEntry{
        fromUser, Clean(std::move(text), MaximumEntryLength)});
    if (history_.size() > MaximumChatEntries)
        history_.erase(history_.begin(),
            history_.begin() + static_cast<std::ptrdiff_t>(
                history_.size() - MaximumChatEntries));
    historyFirst_ = history_.size() > CardsPerPage
        ? history_.size() - CardsPerPage : 0;
    bubbleText_ = history_.back().text;
    RefreshBubble();
    RefreshVisibleHistory();
    SaveLocalState();
}

void RenegadeAssistantDock::RefreshBubble()
{
    bubble_.SetText(
        Wrap(bubbleText_.empty()
            ? "I'm here when you need a hand." : bubbleText_, 43, 3));
}

void RenegadeAssistantDock::RefreshGuide()
{
    if (guideStep_ < 0)
    {
        guideTitle_.SetText("GUIDED WORKFLOW");
        guideBody_.SetText(
            Wrap("Animation retargeting is available as a native "
                "step-by-step guide. Choose RETARGET GUIDE to begin.", 43, 5));
        guideEvidence_.SetText("LOCAL VERIFIED STUDIO CONTROLS");
    }
    else
    {
        const auto& step = RetargetSteps[guideStep_];
        guideTitle_.SetText("STEP " + std::to_string(guideStep_ + 1) +
            " / 4  //  " + step.heading);
        guideBody_.SetText(Wrap(step.text, 43, 7));
        guideEvidence_.SetText(
            "SOURCE: " + std::string(step.evidence));
    }
    guidePrevious_.SetEnabled(guideStep_ > 0);
    guideNext_.SetEnabled(
        guideStep_ < static_cast<int>(RetargetSteps.size() - 1));
}

void RenegadeAssistantDock::RefreshVisibleHistory()
{
    historyFirst_ = std::min(historyFirst_, history_.size());
    if (historyFirst_ + CardsPerPage > history_.size() &&
        history_.size() > CardsPerPage)
        historyFirst_ = history_.size() - CardsPerPage;
    for (std::size_t i = 0; i < historyCards_.size(); ++i)
    {
        const std::size_t index = historyFirst_ + i;
        if (index < history_.size())
        {
            const auto& item = history_[index];
            historyCards_[i].SetText(
                std::string(item.fromUser ? "YOU // " : "RENEGADE // ") +
                Wrap(item.text, 39, 4));
        }
        else
            historyCards_[i].SetText("");
    }
    historyTitle_.SetText("CHAT HISTORY  //  " +
        std::to_string(history_.size()) + " MESSAGES");
    older_.SetEnabled(historyFirst_ > 0);
    newer_.SetEnabled(historyFirst_ + CardsPerPage < history_.size());
}

void RenegadeAssistantDock::SaveLocalState() const
{
    std::error_code ec;
    std::filesystem::create_directories(HistoryFile.parent_path(), ec);
    if (ec)
        return;
    std::ofstream stream(HistoryFile, std::ios::trunc);
    if (stream)
    {
        stream << "RENEGADE_ASSISTANT_HISTORY_V1\n";
        for (const auto& item : history_)
            stream << (item.fromUser ? 'U' : 'A') << ' '
                << std::quoted(item.text) << '\n';
    }
    std::ofstream guideStream(GuideFile, std::ios::trunc);
    if (guideStream)
        guideStream << guideStep_ << '\n';
}

void RenegadeAssistantDock::LoadLocalState()
{
    std::ifstream stream(HistoryFile);
    std::string header;
    if (stream && std::getline(stream, header) &&
        header == "RENEGADE_ASSISTANT_HISTORY_V1")
    {
        char role = 0;
        std::string text;
        while (stream >> role >> std::quoted(text))
        {
            if ((role != 'U' && role != 'A') ||
                text.size() > MaximumEntryLength)
                break;
            history_.push_back(ChatEntry{role == 'U', text});
            if (history_.size() > MaximumChatEntries)
                history_.erase(history_.begin());
        }
    }
    std::ifstream guideStream(GuideFile);
    int step = -1;
    if (guideStream >> step && step >= -1 &&
        step < static_cast<int>(RetargetSteps.size()))
        guideStep_ = step;
    historyFirst_ = history_.size() > CardsPerPage
        ? history_.size() - CardsPerPage : 0;
    if (!history_.empty())
        bubbleText_ = history_.back().text;
    RefreshBubble();
}
}
