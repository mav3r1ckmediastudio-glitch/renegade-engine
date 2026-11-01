#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <WickedEngine.h>

#include "RenegadeScreenChoicePicker.h"
#include "RenegadeScreenColorPicker.h"
#include "RenegadeScreenDataBindingInspector.h"
#include "RenegadeScreenCompositionInspector.h"
#include "RenegadeScreenLayoutInspector.h"
#include "RenegadeScreenAnimationInspector.h"
#include "RenegadeScreenSimulationInspector.h"
#include "RenegadeScreenThemeInspector.h"
#include "RenegadeScreenControlInspector.h"
#include "RenegadeScreenComponentInspector.h"
#include "RenegadeScreenInteractionInspector.h"
#include "RenegadeGameplayAttributeInspector.h"
#include "RenegadeScreenEditorWorkspace.h"
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenCreatorResourceService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/CreatorScreenFontImportService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ScreenPerformanceBudget.h"
#include <fstream>
#include <iterator>

namespace renegade::studio
{
    // Gate 8D's creator Inspector. It overlays the accepted 8C basic Inspector
    // only while expanded. All persistent changes still flow through
    // ScreenAuthoringSession and the shared Runtime renderer remains preview
    // authority.
    class RenegadeScreenEditorAdvancedInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Renegade Screen Advanced Inspector");
            gameplayAttributeInspector_.Create();
            interactionInspector_.Create();interactionInspector_.SetActive(false);
            componentInspector_.Create();componentInspector_.SetActive(false);
            controlInspector_.Create();controlInspector_.SetActive(false);
            themeInspector_.Create();themeInspector_.SetActive(false);
            simulationInspector_.Create();simulationInspector_.SetActive(false);
            animationInspector_.Create();animationInspector_.SetActive(false);
            layoutInspector_.Create();layoutInspector_.SetActive(false);
            compositionInspector_.Create();
            compositionInspector_.SetActive(false);
            layoutInspector_.SetActive(false);
            dataInspector_.Create();
            dataInspector_.SetActive(false);
            SetShadowRadius(0.0f);

            toggleButton_.Create("Screen Advanced Inspector Toggle");
            toggleButton_.SetText("ADVANCED");
            toggleButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                expanded_ = !expanded_;
                toggleButton_.SetText(expanded_ ? "< BASIC" : "ADVANCED");
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });

            stylePageButton_.Create("Screen Inspector Style Page");
            stylePageButton_.SetText("STYLE");
            stylePageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Style;
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });
            textPageButton_.Create("Screen Inspector Text Page");
            textPageButton_.SetText("TEXT / FONT");
            textPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Text;
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });
            bindingPageButton_.Create("Screen Inspector Binding Page");
            bindingPageButton_.SetText("FLOW");
            bindingPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Binding;
                choicePicker_.Close();
                colorPicker_.Close();
                RefreshFromSelection(true);
            });

            dataPageButton_.Create("Screen Inspector Gameplay Data Page");
            dataPageButton_.SetText("DATA");
            dataPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_ = Page::Data;
                choicePicker_.Close();colorPicker_.Close();
                RefreshFromSelection(true);
            });

            compositionPageButton_.Create("Screen Inspector Composition Page");
            compositionPageButton_.SetText("SCREEN");
            compositionPageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                page_=Page::Composition;choicePicker_.Close();colorPicker_.Close();
                dataInspector_.ClosePopup();compositionInspector_.Bind(session_,documentChanged_);
            });

            layoutPageButton_.Create("Screen Inspector Layout Page");
            layoutPageButton_.SetText("LAYOUT");
            layoutPageButton_.OnClick([this](const wi::gui::EventArgs&){
                page_=Page::Layout;choicePicker_.Close();colorPicker_.Close();
                dataInspector_.ClosePopup();RefreshFromSelection(true);
            });

            interactionPageButton_.Create("Ordered UI interactions page");interactionPageButton_.SetText("INTERACTIONS");
            interactionPageButton_.OnClick([this](const wi::gui::EventArgs&){
                page_=Page::Interaction;choicePicker_.Close();colorPicker_.Close();dataInspector_.ClosePopup();
                RefreshFromSelection(true);
            });
            componentPageButton_.Create("Linked component page");componentPageButton_.SetText("COMPONENT");
            componentPageButton_.OnClick([this](const wi::gui::EventArgs&){
                page_=Page::Component;choicePicker_.Close();colorPicker_.Close();dataInspector_.ClosePopup();
                RefreshFromSelection(true);
            });
            controlPageButton_.Create("Screen control value page");controlPageButton_.SetText("CONTROL / INITIAL VALUE");
            controlPageButton_.OnClick([this](const wi::gui::EventArgs&){
                page_=Page::Control;choicePicker_.Close();colorPicker_.Close();dataInspector_.ClosePopup();
                RefreshFromSelection(true);
            });
            themePageButton_.Create("Screen theme page");themePageButton_.SetText("THEME");
            themePageButton_.OnClick([this](const wi::gui::EventArgs&){
                page_=Page::Theme;choicePicker_.Close();colorPicker_.Close();dataInspector_.ClosePopup();
                controlInspector_.Bind(session_,selectedId_,documentChanged_);
            themeInspector_.Bind(session_,selectedId_,projectRoot_,documentChanged_);
            });
            simulationPageButton_.Create("Screen isolated simulation page");
            simulationPageButton_.SetText("SIMULATE");
            simulationPageButton_.OnClick([this](const wi::gui::EventArgs&){
                page_=Page::Simulation;choicePicker_.Close();colorPicker_.Close();
                dataInspector_.ClosePopup();RefreshFromSelection(true);
            });
            animationPageButton_.Create("Screen animation page");
            animationPageButton_.SetText("ANIMATION");
            animationPageButton_.OnClick([this](const wi::gui::EventArgs&){
                page_=Page::Animation;choicePicker_.Close();colorPicker_.Close();
                dataInspector_.ClosePopup();RefreshFromSelection(true);
            });

            stateButton_.Create("Screen Visual State");
            stateButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                state_ = static_cast<VisualState>(
                    (static_cast<int>(state_) + 1) % 5);
                RefreshFromSelection(true);
            });
            stateImageButton_.Create("Screen State Image Resource");
            stateImageButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenStateImagePicker();
            });

            ConfigureInput(backgroundInput_, "Screen State Background", "BACKGROUND RGBA");
            ConfigureInput(foregroundInput_, "Screen State Foreground", "FOREGROUND RGBA");
            ConfigureInput(tintInput_, "Screen State Tint", "IMAGE TINT RGBA");
            ConfigureInput(borderColorInput_, "Screen Border Color", "BORDER RGBA");
            ConfigureInput(borderWidthInput_, "Screen Border Width", "BORDER WIDTH");
            ConfigureInput(cornerRadiusInput_, "Screen Corner Radius", "CORNER RADIUS");
            ConfigureInput(opacityInput_, "Screen Opacity", "OPACITY 0..1");
            ConfigureColorButton(backgroundColorButton_, "Screen Background Color Picker",
                [this]() { OpenStateColor(ColorTarget::Background); });
            ConfigureColorButton(foregroundColorButton_, "Screen Foreground Color Picker",
                [this]() { OpenStateColor(ColorTarget::Foreground); });
            ConfigureColorButton(tintColorButton_, "Screen Image Tint Color Picker",
                [this]() { OpenStateColor(ColorTarget::Tint); });
            ConfigureColorButton(borderColorButton_, "Screen Border Color Picker",
                [this]() { OpenStateColor(ColorTarget::Border); });
            applyStyleButton_.Create("Apply Screen Style");
            applyStyleButton_.SetText("APPLY STYLE STATE");
            applyStyleButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyStyle();
            });

            fontButton_.Create("Screen Font Resource");
            fontButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenFontPicker();
            });
            ConfigureInput(fontSizeInput_, "Screen Font Size", "FONT SIZE");
            ConfigureInput(characterSpacingInput_, "Screen Character Spacing", "CHAR SPACING");
            ConfigureInput(lineSpacingInput_, "Screen Line Spacing", "LINE SPACING");
            ConfigureInput(softnessInput_, "Screen Font Softness", "SOFTNESS 0..1");
            ConfigureInput(boldenInput_, "Screen Font Bolden", "BOLDEN 0..1");
            ConfigureInput(shadowColorInput_, "Screen Shadow Color", "SHADOW RGBA");
            ConfigureInput(shadowXInput_, "Screen Shadow X", "SHADOW X");
            ConfigureInput(shadowYInput_, "Screen Shadow Y", "SHADOW Y");
            ConfigureInput(shadowSoftnessInput_, "Screen Shadow Softness", "SHADOW SOFTNESS");
            ConfigureInput(shadowBoldenInput_, "Screen Shadow Bolden", "SHADOW BOLDEN");
            ConfigureColorButton(shadowColorButton_, "Screen Shadow Color Picker",
                [this]() { OpenStateColor(ColorTarget::Shadow); });

            horizontalButton_.Create("Screen Horizontal Alignment");
            horizontalButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                EnsureDraftStyle(*widget);
                auto& value = draftStyle_.text.horizontalAlignment;
                value = value == bridge::ScreenHorizontalAlignment::Left
                    ? bridge::ScreenHorizontalAlignment::Center
                    : value == bridge::ScreenHorizontalAlignment::Center
                        ? bridge::ScreenHorizontalAlignment::Right
                        : bridge::ScreenHorizontalAlignment::Left;
                horizontalButton_.SetText(HorizontalText(value));
            });
            verticalButton_.Create("Screen Vertical Alignment");
            verticalButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                EnsureDraftStyle(*widget);
                auto& value = draftStyle_.text.verticalAlignment;
                value = value == bridge::ScreenVerticalAlignment::Top
                    ? bridge::ScreenVerticalAlignment::Center
                    : value == bridge::ScreenVerticalAlignment::Center
                        ? bridge::ScreenVerticalAlignment::Bottom
                        : bridge::ScreenVerticalAlignment::Top;
                verticalButton_.SetText(VerticalText(value));
            });
            wrapButton_.Create("Screen Text Wrap");
            wrapButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* widget = Selected();
                if (!widget) return;
                EnsureDraftStyle(*widget);
                draftStyle_.text.wrap = !draftStyle_.text.wrap;
                wrapButton_.SetText(draftStyle_.text.wrap ? "WRAP: YES" : "WRAP: NO");
            });
            applyTextButton_.Create("Apply Screen Typography");
            applyTextButton_.SetText("APPLY TYPOGRAPHY");
            applyTextButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyText();
            });

            primaryResourceButton_.Create("Screen Primary Image Resource");
            primaryResourceButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenPrimaryImagePicker();
            });
            actionButton_.Create("Screen Action Binding");
            actionButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenActionPicker();
            });
            ConfigureInput(actionIdInput_, "Screen Action ID", "ACTION ID");
            addActionButton_.Create("Add Screen Action");
            addActionButton_.SetText("+ ACTION");
            addActionButton_.OnClick([this](const wi::gui::EventArgs&) { AddAction(); });
            renameActionButton_.Create("Rename Screen Action");
            renameActionButton_.SetText("RENAME ACTION");
            renameActionButton_.OnClick([this](const wi::gui::EventArgs&) { RenameAction(); });
            deleteActionButton_.Create("Delete Screen Action");
            deleteActionButton_.SetText("DELETE ACTION");
            deleteActionButton_.OnClick([this](const wi::gui::EventArgs&) { DeleteAction(); });
            focusEarlierButton_.Create("Screen Focus Earlier");
            focusEarlierButton_.SetText("FOCUS UP");
            focusEarlierButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                MoveFocus(true);
            });
            focusLaterButton_.Create("Screen Focus Later");
            focusLaterButton_.SetText("FOCUS DOWN");
            focusLaterButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                MoveFocus(false);
            });

            parentButton_.Create("Screen Parent Binding");
            parentButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                OpenParentPicker();
            });
            layoutButton_.Create("Screen Layout Mode");
            layoutButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                draftLayout_ = draftLayout_ == bridge::ScreenLayoutMode::Absolute
                    ? bridge::ScreenLayoutMode::Anchored
                    : bridge::ScreenLayoutMode::Absolute;
                layoutButton_.SetText(
                    draftLayout_ == bridge::ScreenLayoutMode::Absolute
                        ? "LAYOUT: ABSOLUTE" : "LAYOUT: ANCHORED");
            });
            ConfigureInput(anchorMinXInput_, "Screen Anchor Min X", "ANCHOR MIN X");
            ConfigureInput(anchorMinYInput_, "Screen Anchor Min Y", "ANCHOR MIN Y");
            ConfigureInput(anchorMaxXInput_, "Screen Anchor Max X", "ANCHOR MAX X");
            ConfigureInput(anchorMaxYInput_, "Screen Anchor Max Y", "ANCHOR MAX Y");
            applyBindingButton_.Create("Apply Screen Binding Layout");
            applyBindingButton_.SetText("APPLY BIND / LAYOUT");
            applyBindingButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                ApplyBinding();
            });

            panelButton_.Create("Create Screen Panel Preset");
            panelButton_.SetText("+ PANEL");
            panelButton_.OnClick([this](const wi::gui::EventArgs&) { CreatePanel(); });
            headingButton_.Create("Create Screen Heading Preset");
            headingButton_.SetText("+ HEADING");
            headingButton_.OnClick([this](const wi::gui::EventArgs&) { CreateHeading(); });
            duplicateComponentButton_.Create("Duplicate Screen Component");
            duplicateComponentButton_.SetText("DUP COMPONENT");
            duplicateComponentButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                DuplicateComponent();
            });

            for (auto* widget : AllControls())
            {
                widget->SetShadowRadius(0.0f);
                widget->SetVisible(false);
            }
            created_ = true;
        }

        void SetBindingActorCatalogue(bridge::ScreenBindingActorCatalogue catalogue)
        { dataInspector_.SetActorCatalogue(catalogue);gameplayAttributeInspector_.SetActorCatalogue(std::move(catalogue)); }

        void OnSetGameplayAttributes(RenegadeGameplayAttributeInspector::AttachCallback callback)
        {setGameplayAttributes_=std::move(callback);}
        void SetGameplayAttributeDocumentReference(std::string id,std::string hint)
        {
            gameplayAttributeDocumentId_=std::move(id);gameplayAttributeDocumentHint_=std::move(hint);
            RefreshGameplayAttributeCatalogue();
        }
        bool HasGameplayAttributeEditor() const noexcept{return gameplayAttributeInspector_.IsOpen();}
        void SetLiveRuntimeProbe(std::function<std::string()> probe)
        {simulationInspector_.SetLiveRuntimeProbe(std::move(probe));}
        void OpenUiBudgets()
        {
            if(!session_||!session_->IsLoaded()||projectRoot_.empty())return;
            choicePicker_.Close();colorPicker_.Close();dataInspector_.ClosePopup();
            expanded_=true;toggleButton_.SetText("< BASIC");
            bridge::ScreenPerformanceBudget budget;std::string error;
            const auto root=projectRoot_,projectId=session_->ProjectId();
            if(!bridge::ReadScreenPerformanceBudget(root,projectId,budget,error))
            {status_="UI BUDGET SETTINGS UNAVAILABLE // "+error;return;}
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto us:{500,1000,2000,5000,10000})
                choices.push_back({"BINDING CPU WARNING: "+std::to_string(us)+" us","cpu:"+std::to_string(us)});
            for(const auto count:{64,128,256})
                choices.push_back({"PRESENTED WIDGET WARNING: "+std::to_string(count),"widgets:"+std::to_string(count)});
            for(const auto count:{32,64,128,512})
                choices.push_back({"AUTHORED TRACK WARNING: "+std::to_string(count),"tracks:"+std::to_string(count)});
            choicePicker_.Open("PROJECT UI WARNING BUDGETS",std::move(choices),"cpu:"+std::to_string(budget.bindingCpuUs),
                [this,root,projectId](const std::string& value)
            {
                if(!session_||!session_->IsLoaded()||projectRoot_!=root||session_->ProjectId()!=projectId)return;
                bridge::ScreenPerformanceBudget current;std::string error;
                if(!bridge::ReadScreenPerformanceBudget(root,projectId,current,error))
                {status_="UI BUDGET SETTINGS UNAVAILABLE // "+error;return;}
                const auto colon=value.find(':');if(colon==std::string::npos)return;
                unsigned amount=0;std::istringstream parser(value.substr(colon+1));parser>>amount;
                if(parser.fail()||parser.peek()!=std::char_traits<char>::eof())return;
                if(value.rfind("cpu:",0)==0)current.bindingCpuUs=amount;
                else if(value.rfind("widgets:",0)==0)current.widgetWarning=amount;
                else if(value.rfind("tracks:",0)==0)current.animationWarning=amount;
                else return;
                if(!bridge::WriteScreenPerformanceBudget(root,projectId,current,error))
                {status_="UI BUDGET NOT SAVED // "+error;return;}
                status_="UI WARNING BUDGET SAVED // NEXT TEST GAME START // NOT A GAMEPLAY CAP";
                wi::backlog::post(status_);
            });
        }
        void OpenGameplayAttributes()
        {
            if(!session_||!session_->IsLoaded())return;
            choicePicker_.Close();colorPicker_.Close();dataInspector_.ClosePopup();themeInspector_.ClosePopup();
            componentInspector_.ClosePopup();interactionInspector_.ClosePopup();
            expanded_=true;toggleButton_.SetText("< BASIC");
            gameplayAttributeInspector_.SetLayout(width_,height_);
            gameplayAttributeInspector_.Open(projectRoot_,session_->ProjectId(),gameplayAttributeDocumentId_,gameplayAttributeDocumentHint_,
                [this](const std::string& id,const std::string& hint,std::string& error){
                    if(!setGameplayAttributes_){error="Project module attachment is unavailable.";return false;}
                    if(!setGameplayAttributes_(id,hint,error))return false;
                    gameplayAttributeDocumentId_=id;gameplayAttributeDocumentHint_=hint;return true;
                },[this](const bridge::GameplayAttributeDocument& doc){dataInspector_.SetGameplayAttributeDocument(doc);});
        }
        void RefreshGameplayAttributeCatalogue()
        {
            dataInspector_.SetGameplayAttributeDocument(std::nullopt);
            if(!session_||!session_->IsLoaded()||gameplayAttributeDocumentId_.empty())return;
            std::string path,error;bridge::GameplayAttributeDocument doc;
            if(bridge::ResolveGameplayAttributeDocumentPath(projectRoot_,session_->ProjectId(),gameplayAttributeDocumentId_,
                gameplayAttributeDocumentHint_,path,error)&&bridge::ReadGameplayAttributeDocument(path,session_->ProjectId(),doc,error))
                dataInspector_.SetGameplayAttributeDocument(std::move(doc));
            else status_="Gameplay attribute catalogue: "+error;
        }

        void Bind(
            bridge::ScreenAuthoringSession* session,
            RenegadeScreenEditorWorkspace* workspace,
            std::string projectRoot,
            std::function<void()> documentChanged)
        {
            session_ = session;
            workspace_ = workspace;
            projectRoot_ = std::move(projectRoot);
            documentChanged_ = std::move(documentChanged);
            compositionInspector_.Bind(session_,documentChanged_);
            simulationInspector_.Bind(session_,projectRoot_);
            RefreshGameplayAttributeCatalogue();
            selectedId_.clear();
            status_ = "ADVANCED INSPECTOR READY";
            RefreshResources();
            RefreshFromSelection(true);
        }

        void Clear() noexcept
        {
            gameplayAttributeInspector_.Clear();
            animationInspector_.ResetPreview();
            simulationInspector_.Clear();
            choicePicker_.Close();
            colorPicker_.Close();
            session_ = nullptr;
            workspace_ = nullptr;
            projectRoot_.clear();
            documentChanged_ = {};
            selectedId_.clear();
            imageResources_.clear();
            fontResources_.clear();
            expanded_ = false;
            draftStyleValid_ = false;
            HideAll();
        }

        [[nodiscard]] bool IsExpanded() const noexcept { return expanded_; }
        [[nodiscard]] bool HasOpenPopup() const noexcept
        {
            return gameplayAttributeInspector_.IsOpen() || choicePicker_.IsOpen() || colorPicker_.IsOpen() || dataInspector_.HasOpenPopup() || themeInspector_.HasOpenPopup() || componentInspector_.HasOpenPopup() || interactionInspector_.HasOpenPopup();
        }


        void OpenAppearanceForSelection()
        {
            if (!session_ || !workspace_) return;
            selectedId_ = workspace_->SelectedWidgetId();
            expanded_ = true;
            // Text/Heading/Button selections should open typography first;
            // other elements open the visual style page.
            const auto* widget = Selected();
            page_ = widget &&
                (widget->kind == bridge::ScreenWidgetKind::Text || bridge::IsScreenInteractive(widget->kind))
                ? Page::Text : Page::Style;
            toggleButton_.SetText("< BASIC");
            RefreshFromSelection(true);
        }

        void OpenProgressArtworkForSelection(bool frame)
        {
            if (!session_ || !workspace_) return;
            selectedId_ = workspace_->SelectedWidgetId();
            const auto* widget = Selected();
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Progress) return;
            const auto targetId = selectedId_;
            const auto screenId = session_->Document().envelope.documentId;
            const std::string current = frame ? widget->progressFrameResourcePath : widget->progressFillResourcePath;
            choicePicker_.Open(frame ? "CHOOSE FRAME IMAGE" : "CHOOSE FILL IMAGE",
                ImageChoices(true), current,
                [this, targetId, screenId, frame](const std::string& value)
                {
                    if (!session_ || !session_->IsLoaded() || selectedId_ != targetId ||
                        session_->Document().envelope.documentId != screenId) return;
                    const auto* selected = Selected();
                    if (!selected || selected->kind != bridge::ScreenWidgetKind::Progress) return;
                    if (value == "::import-image::")
                    { OpenComputerImage(false, frame ? 1 : 2); return; }
                    std::string error;
                    if (!session_->UpdateProgressArtwork(targetId,
                        frame ? value : selected->progressFrameResourcePath,
                        frame ? selected->progressFillResourcePath : value, error))
                    { status_ = "PROGRESS ARTWORK REJECTED // " + error; return; }
                    status_ = "PROGRESS ARTWORK UPDATED // AUTOSAVE PENDING";
                    if (documentChanged_) documentChanged_();
                    RefreshFromSelection(true);
                });
        }

        void ChangeGraphicForSelection(bool allSlots) {
            if(!session_||!workspace_)return;
            selectedId_=workspace_->SelectedWidgetId();
            RefreshFromSelection(true);
            OpenComputerImage(false,0,true,allSlots);
        }
        void OpenImageForSelection()
        {
            if (!session_ || !workspace_) return;
            selectedId_ = workspace_->SelectedWidgetId();
            RefreshFromSelection(true);
            OpenPrimaryImagePicker();
        }

        void SetPreferredPanelWidth(const float width)
        {
            preferredPanelWidth_ = std::clamp(width, 340.0f, 640.0f);
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            panelWidth_ = std::min(preferredPanelWidth_, width_);
            panelX_ = width_ - panelWidth_;
            panelY_ = 64.0f;
            panelHeight_ = std::max(1.0f, height_ - 108.0f);
            // Controls use absolute canvas coordinates, including the header
            // toggle above the right panel. The root clip must contain both.
            SetPos(XMFLOAT2(0.0f, 0.0f));
            SetSize(XMFLOAT2(width_, height_));
            choicePicker_.SetLayout(
                panelX_ + 8.0f, panelY_ + 74.0f,
                std::max(220.0f, panelWidth_ - 16.0f),
                std::min(430.0f, std::max(180.0f, panelHeight_ - 92.0f)));
            colorPicker_.SetLayout(
                panelX_ + 12.0f, panelY_ + 82.0f,
                std::max(280.0f, panelWidth_ - 24.0f),
                std::min(340.0f, std::max(280.0f, panelHeight_ - 110.0f)));
            gameplayAttributeInspector_.SetLayout(width_,height_);
            LayoutControls();
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            Widget::Update(canvas, dt);
            if (!created_ || !IsVisible() || !session_ || !session_->IsLoaded())
            {
                HideAll();
                choicePicker_.Close();
                colorPicker_.Close();
                return;
            }

            if(gameplayAttributeInspector_.IsOpen()){gameplayAttributeInspector_.Update(canvas,dt);return;}

            const bridge::StableId selection = workspace_
                ? workspace_->SelectedWidgetId() : bridge::StableId{};
            if (selection != selectedId_ || observedRevision_ != session_->Revision())
            {
                selectedId_ = selection;
                choicePicker_.Close();
                colorPicker_.Close();
                // BASIC inventory editing should not rebind every specialist
                // inspector on every selection while Advanced is collapsed.
                if(expanded_ || gameplayAttributeInspector_.IsOpen())
                    RefreshFromSelection(true);
                else observedRevision_ = session_->Revision();
            }

            LayoutControls();
            SyncVisibility();
            if(themeInspector_.HasOpenPopup())
            {themeInspector_.Update(canvas,dt);return;}
            if (dataInspector_.HasOpenPopup())
            {
                dataInspector_.Update(canvas,dt);
                return;
            }
            if (choicePicker_.IsOpen())
            {
                choicePicker_.Update();
                return;
            }
            if (colorPicker_.IsOpen())
            {
                colorPicker_.Update();
                return;
            }
            for (auto* widget : AllControls())
            {
                if (widget->IsVisible()) widget->Update(canvas, dt);
            }
            dataInspector_.Update(canvas,dt);
            compositionInspector_.Update(canvas,dt);
            interactionInspector_.Update(canvas,dt);
            componentInspector_.Update(canvas,dt);
            controlInspector_.Update(canvas,dt);
            themeInspector_.Update(canvas,dt);
            simulationInspector_.Update(canvas,dt);
            animationInspector_.Update(canvas,dt);
            layoutInspector_.Update(canvas,dt);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!created_ || !IsVisible() || !session_ || !session_->IsLoaded())
                return;

            ApplyScissor(canvas, scissorRect, cmd);
            if (expanded_)
            {
                DrawRect(panelX_, panelY_, panelWidth_, panelHeight_,
                    wi::Color(7, 11, 15, 255), cmd);
                DrawBorder(panelX_, panelY_, panelWidth_, panelHeight_,
                    wi::Color(39, 183, 222, 255), cmd);
                DrawLabel("ADVANCED SCREEN INSPECTOR",
                    panelX_ + 14.0f, panelY_ + 12.0f, 10,
                    wi::Color(244, 244, 244, 255), cmd);
                DrawLabel(status_, panelX_ + 14.0f,
                    panelY_ + panelHeight_ - 22.0f, 7,
                    wi::Color(135, 151, 159, 255), cmd);
            }
            for (const auto* widget : AllControlsConst())
            {
                if (widget->IsVisible())
                {
                    ApplyScissor(canvas, scissorRect, cmd);
                    widget->Render(canvas, cmd);
                }
            }
            ApplyScissor(canvas, scissorRect, cmd);
            compositionInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            interactionInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            componentInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            controlInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            themeInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            simulationInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            animationInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            layoutInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            dataInspector_.Render(canvas,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            choicePicker_.Render(cmd);
            colorPicker_.Render(cmd);
            ApplyScissor(canvas,scissorRect,cmd);gameplayAttributeInspector_.Render(canvas,cmd);
        }

        bool IsSimulationActive() const noexcept{return simulationInspector_.Active();}
        const char* PreviewModeLabel() const noexcept{return simulationInspector_.Live()?
            "LIVE TEST // RUNTIME VALUES OR EXPLICIT UNAVAILABLE // READ ONLY":
            "SIMULATE // ISOLATED SAMPLES // NO GAMEPLAY ACTIONS";}
        void ResetSimulationPresentation() noexcept {simulationRevisionSeen_=~std::uint64_t(0);}
        void ApplySimulationPreview(screen::ScreenRenderer& renderer)
        {
            if(!session_||!session_->IsLoaded())return;
            const auto revision=simulationInspector_.Revision();
            if(simulationDocumentSeen_==session_->Revision()&&simulationRevisionSeen_==revision)return;
            simulationDocumentSeen_=session_->Revision();simulationRevisionSeen_=revision;
            bridge::ScreenBindingFrame frame;std::string error;
            bridge::ScreenDocument effective;
            if(!bridge::ResolveScreenTheme(projectRoot_,session_->Document(),effective,error))return;
            if(simulationInspector_.Active()&&
                simulationInspector_.Evaluate(effective,frame,error))
            {
                simulationInspector_.SetFrameStatus(frame);
                for(const auto& w:frame.widgets)
                {renderer.SetWidgetState(w.id,w.visible,w.enabled);renderer.SetWidgetFill(w.id,w.fill);renderer.SetWidgetText(w.id,w.text);}
            }
            else for(const auto& w:effective.widgets)
            {renderer.SetWidgetState(w.id,w.visible,w.enabled);renderer.SetWidgetFill(w.id,w.fill);renderer.SetWidgetText(w.id,w.text);}
        }

        bridge::ScreenAnimationPreview AnimationPreview() const noexcept {return animationInspector_.Preview();}
        bridge::ScreenTransitionFrame TransitionPreview() const {return animationInspector_.TransitionPreview();}
        void TickAnimationPreview(float dt){animationInspector_.Tick(dt);simulationInspector_.TickLive(dt);}
        void ResetAnimationPreview(){animationInspector_.ResetPreview();}

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeScreenEditorAdvancedInspector";
        }

    private:
        enum class Page { Style, Text, Binding, Data, Composition, Layout, Animation, Simulation, Theme, Control, Component, Interaction };
        enum class VisualState { Normal, Hover, Pressed, Focused, Disabled };
        enum class ColorTarget { Background, Foreground, Tint, Border, Shadow };

        static void ConfigureInput(
            RenegadeTextInputField& input,
            const char* name,
            const char* description)
        {
            input.Create(name);
            input.SetDescription((std::string(description) + "  ").c_str());
            input.SetCancelInputEnabled(false);
            input.SetShadowRadius(0.0f);
        }

        void ConfigureColorButton(
            RenegadeButton& button,
            const char* name,
            std::function<void()> callback)
        {
            button.Create(name);
            button.SetText("PICK");
            button.OnClick([callback = std::move(callback)](const wi::gui::EventArgs&)
            {
                if (callback) callback();
            });
        }

        static std::string Trim(std::string value)
        {
            const auto whitespace = [](const unsigned char character)
            {
                return std::isspace(character) != 0;
            };
            while (!value.empty() && whitespace(value.front()))
                value.erase(value.begin());
            while (!value.empty() && whitespace(value.back()))
                value.pop_back();
            return value;
        }

        static std::string Number(const float value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        static bool ParseNumber(const std::string& value, float& result)
        {
            try
            {
                std::size_t used = 0;
                const std::string trimmed = Trim(value);
                result = std::stof(trimmed, &used);
                return used == trimmed.size() && std::isfinite(result);
            }
            catch (...) { return false; }
        }

        static std::string ColorText(const bridge::ScreenColor& color)
        {
            std::ostringstream stream;
            stream << std::hex << std::uppercase << std::setfill('0')
                << std::setw(2) << static_cast<int>(color.red)
                << std::setw(2) << static_cast<int>(color.green)
                << std::setw(2) << static_cast<int>(color.blue)
                << std::setw(2) << static_cast<int>(color.alpha);
            return stream.str();
        }

        static bool ParseColor(std::string text, bridge::ScreenColor& color)
        {
            text = Trim(std::move(text));
            if (!text.empty() && text.front() == '#') text.erase(text.begin());
            if (text.size() != 8) return false;
            if (!std::all_of(text.begin(), text.end(), [](const unsigned char c)
                { return std::isxdigit(c) != 0; })) return false;
            try
            {
                const auto value = static_cast<std::uint32_t>(
                    std::stoul(text, nullptr, 16));
                color.red = static_cast<std::uint8_t>((value >> 24) & 0xFF);
                color.green = static_cast<std::uint8_t>((value >> 16) & 0xFF);
                color.blue = static_cast<std::uint8_t>((value >> 8) & 0xFF);
                color.alpha = static_cast<std::uint8_t>(value & 0xFF);
                return true;
            }
            catch (...) { return false; }
        }

        static const char* StateText(const VisualState state) noexcept
        {
            switch (state)
            {
            case VisualState::Normal: return "STATE: NORMAL";
            case VisualState::Hover: return "STATE: HOVER";
            case VisualState::Pressed: return "STATE: PRESSED";
            case VisualState::Focused: return "STATE: FOCUSED";
            case VisualState::Disabled: return "STATE: DISABLED";
            }
            return "STATE";
        }

        static const char* HorizontalText(
            const bridge::ScreenHorizontalAlignment value) noexcept
        {
            switch (value)
            {
            case bridge::ScreenHorizontalAlignment::Left: return "H: LEFT";
            case bridge::ScreenHorizontalAlignment::Center: return "H: CENTER";
            case bridge::ScreenHorizontalAlignment::Right: return "H: RIGHT";
            }
            return "H";
        }

        static const char* VerticalText(
            const bridge::ScreenVerticalAlignment value) noexcept
        {
            switch (value)
            {
            case bridge::ScreenVerticalAlignment::Top: return "V: TOP";
            case bridge::ScreenVerticalAlignment::Center: return "V: CENTER";
            case bridge::ScreenVerticalAlignment::Bottom: return "V: BOTTOM";
            }
            return "V";
        }

        static std::string Short(const std::string& value)
        {
            if (value.empty()) return "< NONE >";
            if (value.size() <= 30) return value;
            return "..." + value.substr(value.size() - 27);
        }

        static std::string ShortId(const bridge::StableId& value)
        {
            return value.size() <= 8 ? value : value.substr(0, 8);
        }

        const bridge::ScreenWidget* Selected() const noexcept
        {
            return session_ ? session_->FindWidget(selectedId_) : nullptr;
        }

        void EnsureDraftStyle(const bridge::ScreenWidget& widget)
        {
            if (!draftStyleValid_)
            {
                draftStyle_ = widget.style;
                draftStyleValid_ = true;
            }
        }

        bridge::ScreenVisualState* DraftVisual() noexcept
        {
            switch (state_)
            {
            case VisualState::Normal: return &draftStyle_.normal;
            case VisualState::Hover: return &draftStyle_.hover;
            case VisualState::Pressed: return &draftStyle_.pressed;
            case VisualState::Focused: return &draftStyle_.focused;
            case VisualState::Disabled: return &draftStyle_.disabled;
            }
            return &draftStyle_.normal;
        }

        const bridge::ScreenVisualState* Visual(
            const bridge::ScreenWidgetStyle& style) const noexcept
        {
            switch (state_)
            {
            case VisualState::Normal: return &style.normal;
            case VisualState::Hover: return &style.hover;
            case VisualState::Pressed: return &style.pressed;
            case VisualState::Focused: return &style.focused;
            case VisualState::Disabled: return &style.disabled;
            }
            return &style.normal;
        }

        void RefreshResources()
        {
            imageResources_.clear();
            fontResources_.clear();
            if (!session_ || !session_->IsLoaded()) return;
            const auto images = bridge::EnumerateScreenCreatorResources(
                projectRoot_, session_->ProjectId(),
                bridge::ScreenCreatorResourceKind::Image);
            if (images.succeeded)
                imageResources_ = images.choices;
            const auto fonts = bridge::EnumerateScreenCreatorResources(
                projectRoot_, session_->ProjectId(),
                bridge::ScreenCreatorResourceKind::Font);
            if (fonts.succeeded)
                fontResources_ = fonts.choices;
            if (!images.succeeded && !images.error.empty())
                status_ = "RESOURCE CATALOGUE // " + images.error;
            else if (!fonts.succeeded && !fonts.error.empty())
                status_ = "RESOURCE CATALOGUE // " + fonts.error;
        }

        void RefreshFromSelection(const bool overwriteDraft)
        {
            const auto* widget = Selected();
            if (!widget) return;
            observedRevision_ = session_->Revision();
            const bool textCapable = widget->kind == bridge::ScreenWidgetKind::Text || bridge::IsScreenInteractive(widget->kind);
            if (!textCapable && page_ == Page::Text) page_ = Page::Style;
            if(page_==Page::Control&&!bridge::IsScreenValueControl(widget->kind))page_=Page::Style;
            componentInspector_.Bind(session_,selectedId_,projectRoot_,documentChanged_);
            if(page_==Page::Interaction&&!bridge::IsScreenInteractive(widget->kind))page_=Page::Style;
            interactionInspector_.Bind(session_,selectedId_,documentChanged_);

            dataInspector_.Bind(session_,selectedId_,documentChanged_);
            themeInspector_.Bind(session_,selectedId_,projectRoot_,documentChanged_);
            controlInspector_.Bind(session_,selectedId_,documentChanged_);
            animationInspector_.Bind(session_,selectedId_,documentChanged_);
            layoutInspector_.Bind(session_,selectedId_,documentChanged_);
            if (overwriteDraft || !draftStyleValid_)
            {
                draftStyle_ = widget->style;
                draftStyleValid_ = true;
                draftPrimaryResource_ = widget->resourcePath;
                draftAction_ = widget->actionId;
                draftParent_ = widget->parentId;
                draftLayout_ = widget->layoutMode;
                draftAnchors_ = widget->anchors;
            }

            stateButton_.SetText(StateText(state_));
            const auto* state = Visual(draftStyle_);
            stateImageButton_.SetText("STATE IMAGE: " + Short(state->imageResourcePath));
            backgroundInput_.SetValue(ColorText(state->background));
            foregroundInput_.SetValue(ColorText(state->foreground));
            tintInput_.SetValue(ColorText(state->imageTint));
            borderColorInput_.SetValue(ColorText(draftStyle_.borderColor));
            borderWidthInput_.SetValue(Number(draftStyle_.borderWidth));
            cornerRadiusInput_.SetValue(Number(draftStyle_.cornerRadius));
            opacityInput_.SetValue(Number(draftStyle_.opacity));

            fontButton_.SetText("FONT: " + Short(draftStyle_.text.fontResource));
            fontSizeInput_.SetValue(Number(draftStyle_.text.fontSize));
            characterSpacingInput_.SetValue(Number(draftStyle_.text.characterSpacing));
            lineSpacingInput_.SetValue(Number(draftStyle_.text.lineSpacing));
            softnessInput_.SetValue(Number(draftStyle_.text.softness));
            boldenInput_.SetValue(Number(draftStyle_.text.bolden));
            shadowColorInput_.SetValue(ColorText(draftStyle_.text.shadowColor));
            shadowXInput_.SetValue(Number(draftStyle_.text.shadowOffsetX));
            shadowYInput_.SetValue(Number(draftStyle_.text.shadowOffsetY));
            shadowSoftnessInput_.SetValue(Number(draftStyle_.text.shadowSoftness));
            shadowBoldenInput_.SetValue(Number(draftStyle_.text.shadowBolden));
            horizontalButton_.SetText(HorizontalText(draftStyle_.text.horizontalAlignment));
            verticalButton_.SetText(VerticalText(draftStyle_.text.verticalAlignment));
            wrapButton_.SetText(draftStyle_.text.wrap ? "WRAP: YES" : "WRAP: NO");

            primaryResourceButton_.SetText("IMAGE: " + Short(draftPrimaryResource_));
            actionButton_.SetText("ACTION: " +
                (draftAction_.empty() ? std::string("< NONE >") : draftAction_));
            actionIdInput_.SetValue(draftAction_);
            parentButton_.SetText("PARENT: " + ParentLabel(draftParent_));
            layoutButton_.SetText(
                draftLayout_ == bridge::ScreenLayoutMode::Absolute
                    ? "LAYOUT: ABSOLUTE" : "LAYOUT: ANCHORED");
            anchorMinXInput_.SetValue(Number(draftAnchors_.minimumX));
            anchorMinYInput_.SetValue(Number(draftAnchors_.minimumY));
            anchorMaxXInput_.SetValue(Number(draftAnchors_.maximumX));
            anchorMaxYInput_.SetValue(Number(draftAnchors_.maximumY));
        }

        std::string ParentLabel(const bridge::StableId& parentId) const
        {
            if (parentId.empty()) return "CANVAS";
            const auto* parent = session_ ? session_->FindWidget(parentId) : nullptr;
            return parent ? Short(parent->name) : "< MISSING >";
        }

        bool IsDescendantOfSelection(const bridge::ScreenWidget& candidate) const
        {
            bridge::StableId parent = candidate.parentId;
            std::vector<bridge::StableId> visited;
            while (!parent.empty())
            {
                if (parent == selectedId_) return true;
                if (std::find(visited.begin(), visited.end(), parent) != visited.end())
                    return true;
                visited.push_back(parent);
                const auto* widget = session_->FindWidget(parent);
                if (!widget) break;
                parent = widget->parentId;
            }
            return false;
        }

        std::vector<RenegadeScreenChoicePicker::Choice> ImageChoices(
            const bool includeEmpty)
        {
            RefreshResources();
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            if (includeEmpty) choices.push_back({"< NONE >", ""});
            for (const auto& choice : imageResources_)
            {
                choices.push_back({
                    choice.projectRelativePath + "  [" + ShortId(choice.assetId) + "]",
                    choice.projectRelativePath});
            }
            choices.push_back({" + IMPORT IMAGE FROM THIS COMPUTER", "::import-image::"});
            return choices;
        }

        std::vector<RenegadeScreenChoicePicker::Choice> FontChoices()
        {
            RefreshResources();
            std::vector<RenegadeScreenChoicePicker::Choice> choices{
                {"BUILT-IN // Liberation Sans", bridge::BuiltinScreenFont}};
            for (const auto& choice : fontResources_)
            {
                choices.push_back({
                    choice.projectRelativePath + "  [" + ShortId(choice.assetId) + "]",
                    choice.projectRelativePath});
            }
            choices.push_back({
                " + IMPORT TRUETYPE FONT FROM THIS COMPUTER",
                "::import-ttf-font::"});
            return choices;
        }

        void OpenStateImagePicker()
        {
            const auto* widget = Selected();
            if (!widget) return;
            EnsureDraftStyle(*widget);
            const std::string current = DraftVisual()->imageResourcePath;
            choicePicker_.Open("SELECT STATE IMAGE", ImageChoices(true), current,
                [this](const std::string& value)
                {
                    if (!Selected()) return;
                    if (value == "::import-image::")
                    {
                        OpenComputerImage(true);
                        return;
                    }
                    DraftVisual()->imageResourcePath = value;
                    stateImageButton_.SetText("STATE IMAGE: " + Short(value));
                    CommitCreatorEdit(CurrentCreatorEdit(),
                        "STATE IMAGE UPDATED // AUTOSAVE PENDING");
                });
        }

        void OpenFontPicker()
        {
            const auto* widget = Selected();
            if (!widget || (widget->kind != bridge::ScreenWidgetKind::Text && !bridge::IsScreenInteractive(widget->kind))) return;
            EnsureDraftStyle(*widget);
            choicePicker_.Open("SELECT FONT", FontChoices(),
                draftStyle_.text.fontResource,
                [this](const std::string& value)
                {
                    if (!Selected()) return;
                    if (value == "::import-ttf-font::")
                    {
                        OpenComputerFont();
                        return;
                    }
                    draftStyle_.text.fontResource = value;
                    fontButton_.SetText("FONT: " + Short(value));
                    CommitCreatorEdit(CurrentCreatorEdit(),
                        "FONT ASSIGNED // AUTOSAVE PENDING");
                });
        }

        void OpenComputerFont()
        {
            if (!session_ || !session_->IsLoaded() ||
                !Selected() || projectRoot_.empty()) return;
            const auto projectId = session_->ProjectId();
            const auto screenId = session_->Document().envelope.documentId;
            const auto widgetId = selectedId_;
            const std::string root = projectRoot_;
            wi::helper::FileDialogParams dialog;
            dialog.type = wi::helper::FileDialogParams::OPEN;
            dialog.description = "Import TrueType font to Renegade project";
            dialog.extensions = {"ttf"};
            wi::helper::FileDialog(dialog,
                [this, projectId, screenId, widgetId, root](
                    const std::string& external)
                {
                    if (external.empty()) return;
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, projectId, screenId, widgetId,
                         root, external](std::uint64_t)
                        {
                            if (!session_ || !session_->IsLoaded() ||
                                session_->ProjectId() != projectId ||
                                session_->Document().envelope.documentId != screenId ||
                                projectRoot_ != root ||
                                selectedId_ != widgetId ||
                                !session_->FindWidget(widgetId))
                            {
                                status_ = "FONT IMPORT CANCELLED // SCREEN CHANGED";
                                return;
                            }
                            const auto imported =
                                bridge::CreatorScreenFontImportService::Import(
                                    root, projectId, external);
                            if (!imported.succeeded)
                            {
                                status_ = "FONT IMPORT FAILED // " + imported.error;
                                return;
                            }
                            const auto* widget = Selected();
                            if (!widget ||
                                (widget->kind != bridge::ScreenWidgetKind::Text && !bridge::IsScreenInteractive(widget->kind)))
                                return;
                            EnsureDraftStyle(*widget);
                            draftStyle_.text.fontResource = imported.runtimePath;
                            fontButton_.SetText(
                                "FONT: " + Short(imported.runtimePath));
                            CommitCreatorEdit(CurrentCreatorEdit(),
                                "CUSTOM FONT IMPORTED + ASSIGNED // AUTOSAVE");
                            RefreshResources();
                        });
                });
        }

        void OpenPrimaryImagePicker()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Image) return;
            choicePicker_.Open("SELECT IMAGE", ImageChoices(true), draftPrimaryResource_,
                [this](const std::string& value)
                {
                    if (value == "::import-image::")
                    {
                        OpenComputerImage(false);
                        return;
                    }
                    draftPrimaryResource_ = value;
                    primaryResourceButton_.SetText("IMAGE: " + Short(value));
                    CommitCreatorEdit(CurrentCreatorEdit(),
                        "IMAGE UPDATED // AUTOSAVE PENDING");
                });
        }

        // Import through Renegade's actual governed texture pipeline, not a
        // stale absolute file path. The result becomes a project source asset
        // and a registered LP08 product that Build Game can resolve.
        void OpenComputerImage(const bool stateImage, const int progressSlot = 0, const bool directGraphic = false, const bool allSlots = false)
        {
            if (!session_ || !session_->IsLoaded() ||
                !Selected() || projectRoot_.empty()) return;
            const auto projectId = session_->ProjectId();
            const auto screenId = session_->Document().envelope.documentId;
            const auto widgetId = selectedId_;
            const auto requestedVisualState = state_;
            const std::string root = projectRoot_;
            wi::helper::FileDialogParams dialog;
            dialog.type = wi::helper::FileDialogParams::OPEN;
            dialog.description = "Import UI image to Renegade project";
            dialog.extensions = {"png", "jpg", "jpeg", "tga",
                                 "bmp", "dds", "hdr"};
            wi::helper::FileDialog(dialog,
                [this, projectId, screenId, widgetId, root, stateImage,
                 requestedVisualState, progressSlot, directGraphic, allSlots](const std::string& path)
                {
                    if (path.empty()) return;
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, projectId, screenId, widgetId, root,
                         stateImage, requestedVisualState, progressSlot, path, directGraphic, allSlots](std::uint64_t)
                        {
                            const auto report=[this,directGraphic](const std::string& message) {
                                status_=message;
                                if(directGraphic&&workspace_)workspace_->ReportGraphicStatus(message,true);
                            };
                            if (!session_ || !session_->IsLoaded() ||
                                session_->ProjectId() != projectId ||
                                session_->Document().envelope.documentId != screenId ||
                                projectRoot_ != root ||
                                selectedId_ != widgetId ||
                                (stateImage && state_ != requestedVisualState) ||
                                (directGraphic && workspace_ && workspace_->SelectedWidgetId()!=widgetId) ||
                                !session_->FindWidget(widgetId))
                            {
                                report("IMAGE IMPORT CANCELLED // SCREEN CHANGED");
                                return;
                            }
                            const auto imported =
                                bridge::CreatorTextureWorkflowService()
                                    .ImportTexture(root, projectId, path);
                            if (!imported.succeeded)
                            {
                                report("IMAGE IMPORT FAILED // " + imported.error);
                                return;
                            }
                            // Mirror governed LP08's retained source to a
                            // Screen-readable Content dependency. The project
                            // owns both the governed product and the packaged
                            // raw image that the ScreenRenderer actually loads.
                            namespace fs = std::filesystem;
                            const fs::path relativeMirror =
                                fs::path("Content") / "UI" / "Images" /
                                fs::u8path(imported.assetId +
                                    fs::u8path(imported.sourceProjectRelativePath)
                                        .extension().generic_u8string());
                            const fs::path physicalMirror =
                                fs::u8path(root) / relativeMirror;
                            const fs::path physicalSource =
                                fs::u8path(root) /
                                    fs::u8path(imported.sourceProjectRelativePath);
                            std::ifstream stream(physicalSource, std::ios::binary);
                            if (!stream)
                            {
                                report("IMAGE IMPORTED BUT RETAINED SOURCE CANNOT BE READ");
                                return;
                            }
                            std::vector<std::uint8_t> bytes(
                                std::istreambuf_iterator<char>(stream), {});
                            std::error_code ec;
                            fs::create_directories(
                                physicalMirror.parent_path(), ec);
                            if (ec || fs::exists(physicalMirror))
                            {
                                report("IMAGE IMPORTED BUT UI DESTINATION IS UNAVAILABLE");
                                return;
                            }
                            bridge::ProjectDocumentWrite mirrorWrite;
                            mirrorWrite.destinationPath =
                                physicalMirror.generic_u8string();
                            mirrorWrite.content = bytes;
                            mirrorWrite.validator = [bytes](
                                const std::string& staged, std::string& error)
                            {
                                std::ifstream verify(
                                    fs::u8path(staged), std::ios::binary);
                                if (!verify)
                                {
                                    error = "Cannot verify imported Screen image.";
                                    return false;
                                }
                                const std::vector<std::uint8_t> actual(
                                    std::istreambuf_iterator<char>(verify), {});
                                if (actual != bytes)
                                {
                                    error = "Staged Screen image bytes differ.";
                                    return false;
                                }
                                error.clear();
                                return true;
                            };
                            bridge::ProjectDocumentTransactionOptions options;
                            options.allowedRoot = root;
                            options.journalDirectory = (
                                fs::u8path(root) / "Intermediate" /
                                "Transactions").generic_u8string();
                            const auto committed =
                                bridge::ProjectDocumentTransaction().Execute(
                                    {std::move(mirrorWrite)}, std::move(options));
                            if (!committed.success)
                            {
                                report("UI IMAGE RESOURCE COPY FAILED // " +
                                    committed.message);
                                return;
                            }
                            const std::string runtimeImage =
                                relativeMirror.generic_u8string();
                            std::string resolved, validationError;
                            if (!bridge::ResolveScreenResourcePath(
                                    root, runtimeImage, resolved, validationError))
                            {
                                report("UI IMAGE IMPORT VALIDATION FAILED // " +
                                    validationError);
                                return;
                            }
                            const auto* widget = Selected();
                            if (!widget) return;
                            if(directGraphic) {
                                std::string graphicError;
                                if(!session_->ChangeWidgetGraphic(widgetId,runtimeImage,allSlots,graphicError)) {
                                    report(graphicError);return;
                                }
                                if(documentChanged_)documentChanged_();
                                if(workspace_)workspace_->ReportGraphicStatus(allSlots?
                                    "All slot graphics updated. Undo restores them.":
                                    "Graphic updated. Layout and actions preserved.",false);
                                RefreshFromSelection(true);
                            }
                            else if (progressSlot != 0)
                            {
                                std::string artError;
                                if (!session_->UpdateProgressArtwork(widgetId,
                                    progressSlot == 1 ? runtimeImage : widget->progressFrameResourcePath,
                                    progressSlot == 2 ? runtimeImage : widget->progressFillResourcePath, artError))
                                { report("PROGRESS IMAGE REJECTED // " + artError); return; }
                                status_ = "PROGRESS IMAGE IMPORTED // AUTOSAVE PENDING";
                                if (documentChanged_) documentChanged_();
                            }
                            else if (stateImage)
                            {
                                EnsureDraftStyle(*widget);
                                DraftVisual()->imageResourcePath = runtimeImage;
                                CommitCreatorEdit(CurrentCreatorEdit(),
                                    "IMAGE IMPORTED + STATE ASSIGNED // AUTOSAVE");
                            }
                            else
                            {
                                draftPrimaryResource_ = runtimeImage;
                                CommitCreatorEdit(CurrentCreatorEdit(),
                                    "IMAGE IMPORTED + ELEMENT ASSIGNED // AUTOSAVE");
                            }
                            RefreshResources();
                        });
                });
        }

        void OpenActionPicker()
        {
            const auto* widget = Selected();
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Button || !session_)
                return;
            std::vector<RenegadeScreenChoicePicker::Choice> choices{
                {"< NONE > // UNBIND BUTTON", ""}};
            for (const auto& action : session_->Document().actions)
                if(!bridge::IsRuntimeUiAccessibilityAction(action.id))
                    choices.push_back({action.id, action.id});
            choices.push_back({"PLAYER ACCESSIBILITY // REDUCED MOTION TOGGLE",
                bridge::RuntimeUiReducedMotionToggleAction});
            choices.push_back({"PLAYER ACCESSIBILITY // REDUCED MOTION ON",
                bridge::RuntimeUiReducedMotionOnAction});
            choices.push_back({"PLAYER ACCESSIBILITY // REDUCED MOTION OFF",
                bridge::RuntimeUiReducedMotionOffAction});
            choices.push_back({"PLAYER ACCESSIBILITY // INCREASE TEXT SIZE",bridge::RuntimeUiTextScaleIncreaseAction});
            choices.push_back({"PLAYER ACCESSIBILITY // DECREASE TEXT SIZE",bridge::RuntimeUiTextScaleDecreaseAction});
            choices.push_back({"PLAYER ACCESSIBILITY // RESET TEXT SIZE",bridge::RuntimeUiTextScaleResetAction});
            choices.push_back({"PLAYER ACCESSIBILITY // CYCLE CONTRAST PALETTE",bridge::RuntimeUiContrastCycleAction});
            choices.push_back({"PLAYER ACCESSIBILITY // CYCLE FOCUS COLOUR",bridge::RuntimeUiFocusColorCycleAction});
            choices.push_back({"PLAYER ACCESSIBILITY // CYCLE FOCUS WIDTH",bridge::RuntimeUiFocusWidthCycleAction});
            choices.push_back({"PLAYER ACCESSIBILITY // INCREASE UI SCALE",bridge::RuntimeUiScaleIncreaseAction});
            choices.push_back({"PLAYER ACCESSIBILITY // DECREASE UI SCALE",bridge::RuntimeUiScaleDecreaseAction});
            choices.push_back({"PLAYER ACCESSIBILITY // RESET UI SCALE",bridge::RuntimeUiScaleResetAction});
            choices.push_back({"PLAYER ACCESSIBILITY // CYCLE TARGET SIZE",bridge::RuntimeUiTargetSizeCycleAction});
            const auto widgetId = selectedId_;
            const auto screenId = session_->Document().envelope.documentId;
            choicePicker_.Open("SELECT BUTTON ACTION", std::move(choices), draftAction_,
                [this, widgetId, screenId](const std::string& value)
                {
                    if (!session_ || !session_->IsLoaded() || selectedId_ != widgetId ||
                        session_->Document().envelope.documentId != screenId ||
                        !session_->FindWidget(widgetId)) return;
                    if(bridge::IsRuntimeUiAccessibilityAction(value))
                    {
                        std::string bindError;
                        if(!session_->BindRuntimeUiAccessibilityAction(widgetId,value,bindError))
                        {status_="ACCESSIBILITY ACTION REJECTED // "+bindError;return;}
                        draftAction_=value;
                        status_="PLAYER ACCESSIBILITY ACTION BOUND // ONE UNDOABLE CHANGE";
                        if(documentChanged_)documentChanged_();
                        RefreshFromSelection(true);
                        return;
                    }
                    // Picker selection is an explicit binding action, not a
                    // draft-only visual change. Go through Undo/autosave authority.
                    draftAction_ = value;
                    CommitCreatorEdit(CurrentCreatorEdit(),
                        value.empty() ? "BUTTON ACTION CLEARED // AUTOSAVE PENDING" :
                            "BUTTON ACTION BOUND // AUTOSAVE PENDING");
                });
        }

        void OpenParentPicker()
        {
            const auto* widget = Selected();
            if (!widget || !session_) return;
            std::vector<RenegadeScreenChoicePicker::Choice> choices{
                {"CANVAS", ""}};
            for (const auto& candidate : session_->Document().widgets)
            {
                if (candidate.id == selectedId_ || IsDescendantOfSelection(candidate))
                    continue;
                choices.push_back({candidate.name + "  [" + ShortId(candidate.id) + "]",
                    candidate.id});
            }
            const auto widgetId = selectedId_;
            const auto screenId = session_->Document().envelope.documentId;
            choicePicker_.Open("SELECT PARENT", std::move(choices), draftParent_,
                [this, widgetId, screenId](const std::string& value)
                {
                    if (!session_ || !session_->IsLoaded() || selectedId_ != widgetId ||
                        session_->Document().envelope.documentId != screenId ||
                        !session_->FindWidget(widgetId)) return;
                    // Parent selection changes screen hierarchy; persist it
                    // through the authoring transaction rather than only
                    // updating the visible draft label.
                    draftParent_ = value;
                    CommitCreatorEdit(CurrentCreatorEdit(),
                        value.empty() ? "WIDGET MOVED TO CANVAS // AUTOSAVE PENDING" :
                            "WIDGET PARENT UPDATED // AUTOSAVE PENDING");
                });
        }

        void OpenStateColor(const ColorTarget target)
        {
            const auto* widget = Selected();
            if (!widget) return;
            EnsureDraftStyle(*widget);
            bridge::ScreenColor value;
            std::string title;
            if (target == ColorTarget::Background)
            {
                value = DraftVisual()->background;
                title = "BACKGROUND RGBA";
            }
            else if (target == ColorTarget::Foreground)
            {
                value = DraftVisual()->foreground;
                title = "FOREGROUND RGBA";
            }
            else if (target == ColorTarget::Tint)
            {
                value = DraftVisual()->imageTint;
                title = "IMAGE TINT RGBA";
            }
            else if (target == ColorTarget::Border)
            {
                value = draftStyle_.borderColor;
                title = "BORDER RGBA";
            }
            else
            {
                value = draftStyle_.text.shadowColor;
                title = "SHADOW RGBA";
            }
            const auto widgetId = selectedId_;
            const auto screenId = session_->Document().envelope.documentId;
            const auto visualState = state_;
            colorPicker_.Open(title, value,
                [this, target, widgetId, screenId, visualState](const bridge::ScreenColor& color)
                {
                    if (!session_ || !session_->IsLoaded() || selectedId_ != widgetId ||
                        session_->Document().envelope.documentId != screenId ||
                        state_ != visualState || !session_->FindWidget(widgetId)) return;
                    if (target == ColorTarget::Background)
                    {
                        DraftVisual()->background = color;
                        backgroundInput_.SetValue(ColorText(color));
                    }
                    else if (target == ColorTarget::Foreground)
                    {
                        DraftVisual()->foreground = color;
                        foregroundInput_.SetValue(ColorText(color));
                    }
                    else if (target == ColorTarget::Tint)
                    {
                        DraftVisual()->imageTint = color;
                        tintInput_.SetValue(ColorText(color));
                    }
                    else if (target == ColorTarget::Border)
                    {
                        draftStyle_.borderColor = color;
                        borderColorInput_.SetValue(ColorText(color));
                    }
                    else
                    {
                        draftStyle_.text.shadowColor = color;
                        shadowColorInput_.SetValue(ColorText(color));
                    }
                });
        }

        bool ReadStyleFields(bridge::ScreenWidgetStyle& style)
        {
            auto* state = [&]() -> bridge::ScreenVisualState*
            {
                switch (state_)
                {
                case VisualState::Normal: return &style.normal;
                case VisualState::Hover: return &style.hover;
                case VisualState::Pressed: return &style.pressed;
                case VisualState::Focused: return &style.focused;
                case VisualState::Disabled: return &style.disabled;
                }
                return &style.normal;
            }();
            if (!ParseColor(backgroundInput_.GetValue(), state->background))
            {
                status_ = "STYLE REJECTED // BACKGROUND RGBA IS INVALID";
                return false;
            }
            if (!ParseColor(foregroundInput_.GetValue(), state->foreground))
            {
                status_ = "STYLE REJECTED // FOREGROUND RGBA IS INVALID";
                return false;
            }
            if (!ParseColor(tintInput_.GetValue(), state->imageTint))
            {
                status_ = "STYLE REJECTED // IMAGE TINT RGBA IS INVALID";
                return false;
            }
            if (!ParseColor(borderColorInput_.GetValue(), style.borderColor))
            {
                status_ = "STYLE REJECTED // BORDER RGBA IS INVALID";
                return false;
            }
            if (!ParseNumber(borderWidthInput_.GetValue(), style.borderWidth))
            {
                status_ = "STYLE REJECTED // BORDER WIDTH IS INVALID";
                return false;
            }
            if (!ParseNumber(cornerRadiusInput_.GetValue(), style.cornerRadius))
            {
                status_ = "STYLE REJECTED // CORNER RADIUS IS INVALID";
                return false;
            }
            if (!ParseNumber(opacityInput_.GetValue(), style.opacity))
            {
                status_ = "STYLE REJECTED // OPACITY IS INVALID";
                return false;
            }
            return true;
        }

        bool ReadTextFields(bridge::ScreenWidgetStyle& style)
        {
            auto& text = style.text;
            if (!ParseNumber(fontSizeInput_.GetValue(), text.fontSize))
            {
                status_ = "TYPOGRAPHY REJECTED // FONT SIZE IS INVALID";
                return false;
            }
            if (!ParseNumber(characterSpacingInput_.GetValue(), text.characterSpacing))
            {
                status_ = "TYPOGRAPHY REJECTED // CHARACTER SPACING IS INVALID";
                return false;
            }
            if (!ParseNumber(lineSpacingInput_.GetValue(), text.lineSpacing))
            {
                status_ = "TYPOGRAPHY REJECTED // LINE SPACING IS INVALID";
                return false;
            }
            if (!ParseNumber(softnessInput_.GetValue(), text.softness))
            {
                status_ = "TYPOGRAPHY REJECTED // SOFTNESS IS INVALID";
                return false;
            }
            if (!ParseNumber(boldenInput_.GetValue(), text.bolden))
            {
                status_ = "TYPOGRAPHY REJECTED // BOLDEN IS INVALID";
                return false;
            }
            if (!ParseColor(shadowColorInput_.GetValue(), text.shadowColor))
            {
                status_ = "TYPOGRAPHY REJECTED // SHADOW RGBA IS INVALID";
                return false;
            }
            if (!ParseNumber(shadowXInput_.GetValue(), text.shadowOffsetX) ||
                !ParseNumber(shadowYInput_.GetValue(), text.shadowOffsetY))
            {
                status_ = "TYPOGRAPHY REJECTED // SHADOW OFFSET IS INVALID";
                return false;
            }
            if (!ParseNumber(shadowSoftnessInput_.GetValue(), text.shadowSoftness) ||
                !ParseNumber(shadowBoldenInput_.GetValue(), text.shadowBolden))
            {
                status_ = "TYPOGRAPHY REJECTED // SHADOW SOFTNESS/BOLDEN IS INVALID";
                return false;
            }
            return true;
        }

        void CommitCreatorEdit(
            bridge::ScreenWidgetCreatorEdit edit,
            const std::string& success)
        {
            std::string error;
            if (!session_->UpdateWidgetCreatorFields(selectedId_, std::move(edit), error))
            {
                status_ = "REJECTED // " + error;
                RefreshFromSelection(true);
                return;
            }
            draftStyleValid_ = false;
            status_ = success;
            if (documentChanged_) documentChanged_();
            RefreshFromSelection(true);
        }

        bridge::ScreenWidgetCreatorEdit CurrentCreatorEdit() const
        {
            const auto* widget = Selected();
            bridge::ScreenWidgetCreatorEdit edit;
            if (!widget) return edit;
            edit.resourcePath = draftPrimaryResource_;
            edit.actionId = draftAction_;
            edit.parentId = draftParent_;
            edit.layoutMode = draftLayout_;
            edit.anchors = draftAnchors_;
            edit.style = draftStyleValid_ ? draftStyle_ : widget->style;
            return edit;
        }

        void ApplyStyle()
        {
            const auto* widget = Selected();
            if (!widget) return;
            EnsureDraftStyle(*widget);
            if (!ReadStyleFields(draftStyle_)) return;
            CommitCreatorEdit(CurrentCreatorEdit(),
                "STYLE STATE UPDATED // UNSAVED SCREEN CHANGE");
        }

        void ApplyText()
        {
            const auto* widget = Selected();
            if (!widget || (widget->kind != bridge::ScreenWidgetKind::Text && !bridge::IsScreenInteractive(widget->kind))) return;
            EnsureDraftStyle(*widget);
            if (!ReadTextFields(draftStyle_)) return;
            CommitCreatorEdit(CurrentCreatorEdit(),
                "TYPOGRAPHY UPDATED // UNSAVED SCREEN CHANGE");
        }

        void ApplyBinding()
        {
            const auto* widget = Selected();
            if (!widget) return;
            if (!ParseNumber(anchorMinXInput_.GetValue(), draftAnchors_.minimumX) ||
                !ParseNumber(anchorMinYInput_.GetValue(), draftAnchors_.minimumY) ||
                !ParseNumber(anchorMaxXInput_.GetValue(), draftAnchors_.maximumX) ||
                !ParseNumber(anchorMaxYInput_.GetValue(), draftAnchors_.maximumY))
            {
                status_ = "BINDING REJECTED // ANCHORS MUST BE NUMBERS";
                return;
            }
            CommitCreatorEdit(CurrentCreatorEdit(),
                "BINDING / LAYOUT UPDATED // UNSAVED SCREEN CHANGE");
        }

        void AddAction()
        {
            if (!session_) return;
            const std::string value = Trim(actionIdInput_.GetValue());
            std::string error;
            if (!session_->AddAction(value, error))
            {
                status_ = "ADD ACTION REJECTED // " + error;
                return;
            }
            // Creating an action does not silently rebind the selected button
            // or change the action that Rename/Delete currently targets.
            status_ = "ACTION ADDED // CHOOSE IT IN ACTION PICKER TO BIND";
            if (documentChanged_) documentChanged_();
        }

        void RenameAction()
        {
            if (!session_ || draftAction_.empty())
            {
                status_ = "RENAME ACTION REJECTED // SELECT AN ACTION FIRST";
                return;
            }
            const std::string replacement = Trim(actionIdInput_.GetValue());
            const std::string original = draftAction_;
            std::string error;
            if (!session_->RenameAction(original, replacement, error))
            {
                status_ = "RENAME ACTION REJECTED // " + error;
                return;
            }
            draftAction_ = replacement;
            actionButton_.SetText("ACTION: " + replacement);
            status_ = "ACTION RENAMED // BUTTON REFERENCES UPDATED";
            if (documentChanged_) documentChanged_();
            RefreshFromSelection(true);
        }

        void DeleteAction()
        {
            if (!session_ || draftAction_.empty())
            {
                status_ = "DELETE ACTION REJECTED // SELECT AN ACTION FIRST";
                return;
            }
            const std::string action = draftAction_;
            std::string error;
            if (!session_->DeleteAction(action, error))
            {
                status_ = "DELETE ACTION REJECTED // " + error;
                return;
            }
            status_ = "ACTION DELETED // UNSAVED SCREEN CHANGE";
            draftAction_.clear();
            actionIdInput_.SetValue("");
            actionButton_.SetText("ACTION: < NONE >");
            if (documentChanged_) documentChanged_();
            RefreshFromSelection(true);
        }

        void MoveFocus(const bool earlier)
        {
            const auto* widget = Selected();
            if (!session_ || !widget || !bridge::IsScreenInteractive(widget->kind))
                return;
            std::string error;
            const bool moved = earlier
                ? session_->MoveButtonFocusEarlier(widget->id, error)
                : session_->MoveButtonFocusLater(widget->id, error);
            if (!moved)
            {
                status_ = "FOCUS ORDER REJECTED // " + error;
                return;
            }
            status_ = earlier
                ? "BUTTON MOVED EARLIER IN FOCUS ORDER"
                : "BUTTON MOVED LATER IN FOCUS ORDER";
            if (documentChanged_) documentChanged_();
        }

        void CreatePanel()
        {
            if (!session_ || !session_->IsLoaded()) return;
            const auto& document = session_->Document();
            bridge::ScreenWidget widget;
            widget.kind = bridge::ScreenWidgetKind::Image;
            widget.name = "Panel";
            widget.visible = true;
            widget.enabled = false;
            widget.rect = {
                document.designWidth * 0.25f,
                document.designHeight * 0.25f,
                document.designWidth * 0.5f,
                document.designHeight * 0.5f,
            };
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                widget.kind, widget.rect.height);
            widget.style.normal.background = {24, 30, 36, 230};
            widget.style.hover = widget.style.normal;
            widget.style.pressed = widget.style.normal;
            widget.style.focused = widget.style.normal;
            widget.style.disabled = widget.style.normal;
            bridge::StableId created;
            std::string error;
            if (!session_->CreateWidget(std::move(widget), false, created, error))
            {
                status_ = "PANEL CREATE REJECTED // " + error;
                return;
            }
            workspace_->SelectWidget(created);
            status_ = "PANEL CREATED // ALL APPEARANCE REMAINS EDITABLE";
            if (documentChanged_) documentChanged_();
        }

        void CreateHeading()
        {
            if (!session_ || !session_->IsLoaded()) return;
            const auto& document = session_->Document();
            bridge::ScreenWidget widget;
            widget.kind = bridge::ScreenWidgetKind::Text;
            widget.name = "Heading";
            widget.text = "HEADING";
            widget.visible = true;
            widget.enabled = false;
            widget.rect = {
                document.designWidth * 0.15f,
                document.designHeight * 0.12f,
                document.designWidth * 0.7f,
                88.0f,
            };
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                widget.kind, widget.rect.height);
            widget.style.text.fontSize = 48.0f;
            widget.style.text.horizontalAlignment =
                bridge::ScreenHorizontalAlignment::Center;
            bridge::StableId created;
            std::string error;
            if (!session_->CreateWidget(std::move(widget), false, created, error))
            {
                status_ = "HEADING CREATE REJECTED // " + error;
                return;
            }
            workspace_->SelectWidget(created);
            status_ = "HEADING CREATED // PRESET VALUES ARE FULLY EDITABLE";
            if (documentChanged_) documentChanged_();
        }

        void DuplicateComponent()
        {
            if (!session_ || selectedId_.empty()) return;
            bridge::StableId duplicated;
            std::string error;
            if (!session_->DuplicateWidgetTree(selectedId_, duplicated, error))
            {
                status_ = "COMPONENT DUPLICATE REJECTED // " + error;
                return;
            }
            workspace_->SelectWidget(duplicated);
            status_ = "COMPONENT SUBTREE DUPLICATED // UNSAVED SCREEN CHANGE";
            if (documentChanged_) documentChanged_();
        }

        void LayoutControls()
        {
            toggleButton_.SetPos(XMFLOAT2(width_ - 122.0f, 14.0f));
            toggleButton_.SetSize(XMFLOAT2(108.0f, 32.0f));
            if (!expanded_) return;

            const float x = panelX_ + 12.0f;
            const float w = panelWidth_ - 24.0f;
            float y = panelY_ + 40.0f;
            const float third = (w - 8.0f) / 3.0f;
            stylePageButton_.SetPos(XMFLOAT2(x, y));
            stylePageButton_.SetSize(XMFLOAT2(third, 28.0f));
            textPageButton_.SetPos(XMFLOAT2(x + third + 4.0f, y));
            textPageButton_.SetSize(XMFLOAT2(third, 28.0f));
            bindingPageButton_.SetPos(XMFLOAT2(x + (third + 4.0f) * 2.0f, y));
            bindingPageButton_.SetSize(XMFLOAT2(third, 28.0f));
            dataPageButton_.SetPos(XMFLOAT2(x,y+32.0f));
            dataPageButton_.SetSize(XMFLOAT2(third,28.0f));
            layoutPageButton_.SetPos(XMFLOAT2(x+third+4.0f,y+32.0f));
            layoutPageButton_.SetSize(XMFLOAT2(third,28.0f));
            compositionPageButton_.SetPos(XMFLOAT2(x+(third+4.0f)*2.0f,y+32.0f));
            compositionPageButton_.SetSize(XMFLOAT2(third,28.0f));
            animationPageButton_.SetPos(XMFLOAT2(x,y+64.0f));
            animationPageButton_.SetSize(XMFLOAT2(third,28.0f));
            themePageButton_.SetPos(XMFLOAT2(x+third+4,y+64.0f));themePageButton_.SetSize(XMFLOAT2(third,28.0f));
            simulationPageButton_.SetPos(XMFLOAT2(x+2*(third+4),y+64.0f));
            simulationPageButton_.SetSize(XMFLOAT2(third,28.0f));
            const bool hasControl=Selected()&&bridge::IsScreenValueControl(Selected()->kind);
            const bool hasInteractions=Selected()&&bridge::IsScreenInteractive(Selected()->kind);
            const float fourthWidth=hasControl?(w-12.0f)/3.0f:hasInteractions?(w-6.0f)*.5f:w;
            controlPageButton_.SetText("CONTROL");
            controlPageButton_.SetPos(XMFLOAT2(x,y+96.0f));controlPageButton_.SetSize(XMFLOAT2(fourthWidth,28.0f));
            componentPageButton_.SetPos(XMFLOAT2(x+(hasControl?fourthWidth+6.0f:0),y+96.0f));
            componentPageButton_.SetSize(XMFLOAT2(fourthWidth,28.0f));
            interactionPageButton_.SetPos(XMFLOAT2(x+(hasControl?2:1)*(fourthWidth+6.0f),y+96.0f));
            interactionPageButton_.SetSize(XMFLOAT2(fourthWidth,28.0f));
            y += 134.0f;
            interactionInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            componentInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            controlInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            themeInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            simulationInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            animationInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            compositionInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            layoutInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));
            dataInspector_.SetLayout(x,y,w,std::max(1.0f,panelY_+panelHeight_-y-36.0f));

            const auto place = [&](wi::gui::Widget& widget, const float h = 28.0f)
            {
                widget.SetPos(XMFLOAT2(x, y));
                widget.SetSize(XMFLOAT2(w, h));
                y += h + 7.0f;
            };
            const auto pair = [&](wi::gui::Widget& a, wi::gui::Widget& b)
            {
                a.SetPos(XMFLOAT2(x, y));
                a.SetSize(XMFLOAT2(w * 0.70f - 3.0f, 28.0f));
                b.SetPos(XMFLOAT2(x + w * 0.70f + 3.0f, y));
                b.SetSize(XMFLOAT2(w * 0.30f - 3.0f, 28.0f));
                y += 35.0f;
            };
            const auto halves = [&](wi::gui::Widget& a, wi::gui::Widget& b)
            {
                a.SetPos(XMFLOAT2(x, y));
                a.SetSize(XMFLOAT2(w * 0.5f - 3.0f, 28.0f));
                b.SetPos(XMFLOAT2(x + w * 0.5f + 3.0f, y));
                b.SetSize(XMFLOAT2(w * 0.5f - 3.0f, 28.0f));
                y += 35.0f;
            };

            if (page_ == Page::Style)
            {
                place(stateButton_);
                place(stateImageButton_);
                pair(backgroundInput_, backgroundColorButton_);
                pair(foregroundInput_, foregroundColorButton_);
                pair(tintInput_, tintColorButton_);
                pair(borderColorInput_, borderColorButton_);
                halves(borderWidthInput_, cornerRadiusInput_);
                place(opacityInput_);
                place(applyStyleButton_, 32.0f);
            }
            else if (page_ == Page::Text)
            {
                place(fontButton_);
                halves(fontSizeInput_, characterSpacingInput_);
                halves(lineSpacingInput_, softnessInput_);
                place(boldenInput_);
                pair(shadowColorInput_, shadowColorButton_);
                halves(shadowXInput_, shadowYInput_);
                halves(shadowSoftnessInput_, shadowBoldenInput_);
                halves(horizontalButton_, verticalButton_);
                place(wrapButton_);
                place(applyTextButton_, 32.0f);
            }
            else if (page_ == Page::Binding)
            {
                place(primaryResourceButton_);
                place(actionButton_);
                place(actionIdInput_);
                halves(addActionButton_, renameActionButton_);
                place(deleteActionButton_);
                halves(focusEarlierButton_, focusLaterButton_);
                place(parentButton_);
                place(layoutButton_);
                halves(anchorMinXInput_, anchorMinYInput_);
                halves(anchorMaxXInput_, anchorMaxYInput_);
                place(applyBindingButton_, 32.0f);
                y += 4.0f;
                halves(panelButton_, headingButton_);
                place(duplicateComponentButton_, 32.0f);
            }
        }

        void SyncVisibility()
        {
            // Set each final visibility once. Hiding active text fields and then
            // showing them every frame deactivates native typing/caret state.
            std::vector<wi::gui::Widget*> visible{&toggleButton_};
            const auto append = [&visible](const std::vector<wi::gui::Widget*>& controls)
            {
                visible.insert(visible.end(), controls.begin(), controls.end());
            };
            if (expanded_)
            {
                append({&compositionPageButton_, &simulationPageButton_, &themePageButton_, &layoutPageButton_});
            }
            if (expanded_ && Selected())
            {
                append({&componentPageButton_});
                if(bridge::IsScreenInteractive(Selected()->kind))append({&interactionPageButton_});
                if(bridge::IsScreenValueControl(Selected()->kind))append({&controlPageButton_});
                append({&stylePageButton_, &textPageButton_, &bindingPageButton_, &dataPageButton_, &layoutPageButton_, &animationPageButton_});
                textPageButton_.SetEnabled(Selected()->kind == bridge::ScreenWidgetKind::Text ||
                    bridge::IsScreenInteractive(Selected()->kind));
                if (page_ == Page::Style) append(StyleControls());
                else if (page_ == Page::Text)
                {
                    if (Selected()->kind == bridge::ScreenWidgetKind::Text ||
                        bridge::IsScreenInteractive(Selected()->kind))
                        append(TextControls());
                }
                else if (page_ == Page::Binding)
                {
                    append(BindingControls());
                    const bool image = Selected()->kind == bridge::ScreenWidgetKind::Image;
                    const bool button = Selected()->kind == bridge::ScreenWidgetKind::Button;
                    primaryResourceButton_.SetEnabled(image);
                    actionButton_.SetEnabled(button);
                    actionIdInput_.SetEnabled(button);
                    addActionButton_.SetEnabled(button);
                    renameActionButton_.SetEnabled(button);
                    deleteActionButton_.SetEnabled(button);
                    focusEarlierButton_.SetEnabled(bridge::IsScreenInteractive(Selected()->kind));
                    focusLaterButton_.SetEnabled(bridge::IsScreenInteractive(Selected()->kind));
                }
            }
            for (auto* control : AllControls())
                control->SetVisible(std::find(visible.begin(), visible.end(), control) != visible.end());
            dataInspector_.SetActive(expanded_ && Selected() && page_ == Page::Data);
            compositionInspector_.SetActive(expanded_ && page_ == Page::Composition);
            layoutInspector_.SetActive(expanded_ && page_ == Page::Layout);
            animationInspector_.SetActive(expanded_ && Selected() && page_==Page::Animation);
            simulationInspector_.SetActive(expanded_ && page_==Page::Simulation);
            themeInspector_.SetActive(expanded_ && page_==Page::Theme);
            interactionInspector_.SetActive(expanded_ && Selected() && bridge::IsScreenInteractive(Selected()->kind) && page_==Page::Interaction);
            componentInspector_.SetActive(expanded_ && Selected() && page_==Page::Component);
            controlInspector_.SetActive(expanded_ && Selected() && bridge::IsScreenValueControl(Selected()->kind) && page_==Page::Control);
        }

        std::vector<wi::gui::Widget*> StyleControls()
        {
            return {&stateButton_, &stateImageButton_,
                &backgroundInput_, &backgroundColorButton_,
                &foregroundInput_, &foregroundColorButton_,
                &tintInput_, &tintColorButton_,
                &borderColorInput_, &borderColorButton_,
                &borderWidthInput_, &cornerRadiusInput_, &opacityInput_,
                &applyStyleButton_};
        }

        std::vector<wi::gui::Widget*> TextControls()
        {
            return {&fontButton_, &fontSizeInput_, &characterSpacingInput_,
                &lineSpacingInput_, &softnessInput_, &boldenInput_,
                &shadowColorInput_, &shadowColorButton_, &shadowXInput_,
                &shadowYInput_, &shadowSoftnessInput_, &shadowBoldenInput_,
                &horizontalButton_, &verticalButton_, &wrapButton_,
                &applyTextButton_};
        }

        std::vector<wi::gui::Widget*> BindingControls()
        {
            return {&primaryResourceButton_, &actionButton_, &actionIdInput_,
                &addActionButton_, &renameActionButton_, &deleteActionButton_,
                &focusEarlierButton_, &focusLaterButton_, &parentButton_,
                &layoutButton_, &anchorMinXInput_, &anchorMinYInput_,
                &anchorMaxXInput_, &anchorMaxYInput_, &applyBindingButton_,
                &panelButton_, &headingButton_, &duplicateComponentButton_};
        }

        std::vector<wi::gui::Widget*> AllControls()
        {
            auto result = std::vector<wi::gui::Widget*>{
                &toggleButton_, &stylePageButton_, &textPageButton_,
                &bindingPageButton_, &dataPageButton_, &compositionPageButton_, &layoutPageButton_, &animationPageButton_, &simulationPageButton_, &themePageButton_, &controlPageButton_, &componentPageButton_, &interactionPageButton_};
            const auto append = [&result](const std::vector<wi::gui::Widget*>& values)
            {
                result.insert(result.end(), values.begin(), values.end());
            };
            append(StyleControls());
            append(TextControls());
            append(BindingControls());
            return result;
        }

        std::vector<const wi::gui::Widget*> AllControlsConst() const
        {
            std::vector<const wi::gui::Widget*> result;
            auto* self = const_cast<RenegadeScreenEditorAdvancedInspector*>(this);
            for (auto* widget : self->AllControls()) result.push_back(widget);
            return result;
        }

        void HideAll()
        {
            for (auto* widget : AllControls()) widget->SetVisible(false);
            dataInspector_.SetActive(false);
            compositionInspector_.SetActive(false);
            layoutInspector_.SetActive(false);
            animationInspector_.SetActive(false);
            simulationInspector_.SetActive(false);
            themeInspector_.SetActive(false);
            interactionInspector_.SetActive(false);
            componentInspector_.SetActive(false);
            controlInspector_.SetActive(false);
        }

        static void DrawRect(
            const float x, const float y, const float w, const float h,
            const wi::Color color, const wi::graphics::CommandList cmd)
        {
            wi::image::Params params(x, y, w, h, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawBorder(
            const float x, const float y, const float w, const float h,
            const wi::Color color, const wi::graphics::CommandList cmd)
        {
            DrawRect(x, y, w, 1.0f, color, cmd);
            DrawRect(x, y + h - 1.0f, w, 1.0f, color, cmd);
            DrawRect(x, y, 1.0f, h, color, cmd);
            DrawRect(x + w - 1.0f, y, 1.0f, h, color, cmd);
        }

        static void DrawLabel(
            const std::string& value, const float x, const float y,
            const int size, const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            wi::font::Params params(x, y, size,
                wi::font::WIFALIGN_LEFT, wi::font::WIFALIGN_TOP,
                color, wi::Color::Transparent());
            params.bolden = 0.12f;
            wi::font::Draw(value, params, cmd);
        }

        bridge::ScreenAuthoringSession* session_ = nullptr;
        RenegadeScreenEditorWorkspace* workspace_ = nullptr;
        std::function<void()> documentChanged_;
        std::string projectRoot_;
        std::uint64_t observedRevision_ = 0;
        bridge::StableId selectedId_;
        bool created_ = false;
        bool expanded_ = false;
        Page page_ = Page::Style;
        VisualState state_ = VisualState::Normal;
        float width_ = 1.0f;
        float height_ = 1.0f;
        float panelX_ = 0.0f;
        float panelY_ = 64.0f;
        float panelWidth_ = 360.0f;
        float preferredPanelWidth_ = 360.0f;
        float panelHeight_ = 1.0f;
        std::string status_;

        bridge::ScreenWidgetStyle draftStyle_;
        bool draftStyleValid_ = false;
        std::string draftPrimaryResource_;
        std::string draftAction_;
        bridge::StableId draftParent_;
        bridge::ScreenLayoutMode draftLayout_ = bridge::ScreenLayoutMode::Absolute;
        bridge::ScreenAnchors draftAnchors_;
        std::vector<bridge::ScreenCreatorResourceChoice> imageResources_;
        std::vector<bridge::ScreenCreatorResourceChoice> fontResources_;
        RenegadeScreenChoicePicker choicePicker_;
        RenegadeScreenColorPicker colorPicker_;

        RenegadeButton toggleButton_;
        RenegadeButton stylePageButton_;
        RenegadeButton textPageButton_;
        RenegadeButton bindingPageButton_;
        RenegadeButton dataPageButton_;
        RenegadeScreenDataBindingInspector dataInspector_;
        RenegadeButton compositionPageButton_;
        RenegadeScreenCompositionInspector compositionInspector_;
        RenegadeGameplayAttributeInspector gameplayAttributeInspector_;
        RenegadeGameplayAttributeInspector::AttachCallback setGameplayAttributes_;
        std::string gameplayAttributeDocumentId_,gameplayAttributeDocumentHint_;
        RenegadeButton interactionPageButton_;
        RenegadeScreenInteractionInspector interactionInspector_;
        RenegadeButton componentPageButton_;
        RenegadeScreenComponentInspector componentInspector_;
        RenegadeButton controlPageButton_;
        RenegadeScreenControlInspector controlInspector_;
        RenegadeButton themePageButton_;
        RenegadeScreenThemeInspector themeInspector_;
        RenegadeButton simulationPageButton_;
        RenegadeScreenSimulationInspector simulationInspector_;
        std::uint64_t simulationDocumentSeen_=~std::uint64_t(0),simulationRevisionSeen_=~std::uint64_t(0);
        RenegadeButton animationPageButton_;
        RenegadeScreenAnimationInspector animationInspector_;
        RenegadeButton layoutPageButton_;
        RenegadeScreenLayoutInspector layoutInspector_;
        RenegadeButton stateButton_;
        RenegadeButton stateImageButton_;
        RenegadeTextInputField backgroundInput_;
        RenegadeButton backgroundColorButton_;
        RenegadeTextInputField foregroundInput_;
        RenegadeButton foregroundColorButton_;
        RenegadeTextInputField tintInput_;
        RenegadeButton tintColorButton_;
        RenegadeTextInputField borderColorInput_;
        RenegadeButton borderColorButton_;
        RenegadeTextInputField borderWidthInput_;
        RenegadeTextInputField cornerRadiusInput_;
        RenegadeTextInputField opacityInput_;
        RenegadeButton applyStyleButton_;
        RenegadeButton fontButton_;
        RenegadeTextInputField fontSizeInput_;
        RenegadeTextInputField characterSpacingInput_;
        RenegadeTextInputField lineSpacingInput_;
        RenegadeTextInputField softnessInput_;
        RenegadeTextInputField boldenInput_;
        RenegadeTextInputField shadowColorInput_;
        RenegadeButton shadowColorButton_;
        RenegadeTextInputField shadowXInput_;
        RenegadeTextInputField shadowYInput_;
        RenegadeTextInputField shadowSoftnessInput_;
        RenegadeTextInputField shadowBoldenInput_;
        RenegadeButton horizontalButton_;
        RenegadeButton verticalButton_;
        RenegadeButton wrapButton_;
        RenegadeButton applyTextButton_;
        RenegadeButton primaryResourceButton_;
        RenegadeButton actionButton_;
        RenegadeTextInputField actionIdInput_;
        RenegadeButton addActionButton_;
        RenegadeButton renameActionButton_;
        RenegadeButton deleteActionButton_;
        RenegadeButton focusEarlierButton_;
        RenegadeButton focusLaterButton_;
        RenegadeButton parentButton_;
        RenegadeButton layoutButton_;
        RenegadeTextInputField anchorMinXInput_;
        RenegadeTextInputField anchorMinYInput_;
        RenegadeTextInputField anchorMaxXInput_;
        RenegadeTextInputField anchorMaxYInput_;
        RenegadeButton applyBindingButton_;
        RenegadeButton panelButton_;
        RenegadeButton headingButton_;
        RenegadeButton duplicateComponentButton_;
    };
}
