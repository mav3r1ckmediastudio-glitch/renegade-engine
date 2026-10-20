#pragma once

#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenThemeStyle.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include <filesystem>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace renegade::bridge
{
    struct ScreenWidgetAuthoringEdit
    {
        std::string name;
        std::string text;
        ScreenRect resolvedRect;
        bool visible = true;
        bool enabled = true;
        std::optional<float> fill;
        std::optional<ScreenFillDirection> fillDirection;
    };

    struct ScreenWidgetCreatorEdit
    {
        std::string resourcePath;
        std::string actionId;
        StableId parentId;
        ScreenLayoutMode layoutMode = ScreenLayoutMode::Absolute;
        ScreenAnchors anchors;
        ScreenWidgetStyle style;
    };

    // Project-owned in-memory clipboard; roots use resolved canvas coordinates.
    struct ScreenWidgetClipboard
    {
        StableId projectId;
        std::vector<ScreenWidget> widgets;
        std::vector<StableId> focusOrder;
    };

    // UI-independent Screen document mutation boundary. Gate 8C established
    // validated property editing/history/persistence; Gate 8D extends the same
    // authority to creator-level element transactions without giving Studio a
    // second mutation path around the governed Screen document.
    class ScreenAuthoringSession final
    {
    public:
        [[nodiscard]] bool Open(
            const std::string& filePath,
            const StableId& expectedProjectId,
            std::string& error);
        [[nodiscard]] bool Reload(std::string& error);
        [[nodiscard]] bool UpdateScreenSettings(
            ScreenUsage usage, bool enabledInGame, std::string& error)
        {
            if (!loaded_) { error = "No Screen is open."; return false; }
            if (Document().usage == usage && Document().enabledInGame == enabledInGame)
            { error.clear(); return true; }
            auto candidate = Document();
            if (candidate.schemaVersion < 3)
            {
                candidate.migratedFromVersion = candidate.schemaVersion;
                candidate.schemaVersion = 3;
            }
            candidate.usage = usage;
            candidate.enabledInGame = enabledInGame;
            return CommitMutation(std::move(candidate), error);
        }
        [[nodiscard]] bool UpdateScreenSafeArea(ScreenSafeArea area,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            if(!ValidateScreenSafeArea(area,Document().designWidth,Document().designHeight,error))
                return false;
            const auto& current=Document().safeArea;
            if(current.left==area.left&&current.top==area.top&&
               current.right==area.right&&current.bottom==area.bottom)
            {error.clear();return true;}
            auto candidate=Document();
            if(candidate.schemaVersion<3)
            {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            candidate.safeArea=area;
            return CommitMutation(std::move(candidate),error);
        }
        [[nodiscard]] bool UpdateWidgetSizeConstraints(
            const StableId& id,ScreenSizeConstraints constraints,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            if(!ValidateScreenSizeConstraints(constraints,error))return false;
            auto candidate=Document();
            const auto found=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),
                [&](const auto& widget){return widget.id==id;});
            if(found==candidate.widgets.end())
            {error="Widget size constraint target is missing.";return false;}
            const auto& current=found->sizeConstraints;
            if(current.minimumWidth==constraints.minimumWidth&&
                current.minimumHeight==constraints.minimumHeight&&
                current.maximumWidth==constraints.maximumWidth&&
                current.maximumHeight==constraints.maximumHeight)
            {error.clear();return true;}
            found->sizeConstraints=constraints;
            if(candidate.schemaVersion<3)
            {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool UpdateScreenComposition(std::optional<ScreenCompositionPolicy> policy, std::string& error)
        {
            if (!loaded_) { error = "No Screen is open."; return false; }
            if (policy && !ValidateScreenCompositionPolicy(*policy,error)) return false;
            if (!policy && !Document().composition) { error.clear(); return true; }
            auto candidate = Document();
            if (candidate.schemaVersion < 3)
            { candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3; }
            candidate.composition=std::move(policy);
            return CommitMutation(std::move(candidate),error);
        }
        [[nodiscard]] bool Save(std::string& error);
        void Clear() noexcept;

        [[nodiscard]] bool UpdateScreenTheme(std::optional<ScreenThemeReference> theme,
            bool inheritAll,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();candidate.theme=std::move(theme);
            if(inheritAll)for(auto& w:candidate.widgets)w.themeOverrides=0;
            if(candidate.schemaVersion<3)
            {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }
        [[nodiscard]] bool UpdateWidgetThemeOverrides(const StableId& id,unsigned mask,std::string& error)
        {
            if(!loaded_||mask>63){error="Invalid theme override request.";return false;}
            auto candidate=Document();
            auto w=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),[&](const auto& value){return value.id==id;});
            if(w==candidate.widgets.end()){error="Theme override target is missing.";return false;}
            w->themeOverrides=mask;
            if(candidate.schemaVersion<3)
            {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        // A single creator transaction adds an approved player accessibility
        // action if needed and binds the Button, without a fake Flow route.
        [[nodiscard]] bool BindRuntimeUiAccessibilityAction(
            const StableId& id,const std::string& action,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            if(!IsRuntimeUiAccessibilityAction(action))
            {error="Choose a supported player accessibility action.";return false;}
            auto candidate=Document();
            auto found=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),
                [&](const auto& w){return w.id==id;});
            if(found==candidate.widgets.end()||found->kind!=ScreenWidgetKind::Button)
            {error="Player accessibility actions require a Button.";return false;}
            const bool declared=std::any_of(candidate.actions.begin(),candidate.actions.end(),
                [&](const auto& item){return item.id==action;});
            if(found->actionId==action&&!found->localOnly&&declared)
            {error.clear();return true;}
            if(!declared)candidate.actions.push_back({action});
            found->actionId=action;
            found->localOnly=false;
            if(candidate.schemaVersion<3)
            {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool SetButtonLocalOnly(const StableId& id,bool localOnly,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();
            auto found=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),[&](const auto& w){return w.id==id;});
            if(found==candidate.widgets.end()||found->kind!=ScreenWidgetKind::Button)
            {error="Flow dispatch mode requires a Button.";return false;}
            if(found->localOnly==localOnly){error.clear();return true;}
            found->localOnly=localOnly;
            if(candidate.schemaVersion<3){candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool UpdateScreenTransitions(ScreenTransitionPair transitions,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            if(!ValidateScreenTransition(transitions.enter,true,error)||
                !ValidateScreenTransition(transitions.exit,false,error))return false;
            auto candidate=Document();candidate.transitions=std::move(transitions);
            if(candidate.schemaVersion<3){candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }
        [[nodiscard]] bool UpdateWidgetTransitionOverride(const StableId& id,
            std::optional<ScreenTransitionPair> transitions,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();
            auto found=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),[&](const auto& w){return w.id==id;});
            if(found==candidate.widgets.end()||found->kind!=ScreenWidgetKind::Button)
            {error="Navigation transition overrides require a Button.";return false;}
            if(transitions&&(!ValidateScreenTransition(transitions->enter,true,error)||
                !ValidateScreenTransition(transitions->exit,false,error)))return false;
            found->transitionOverride=std::move(transitions);
            if(candidate.schemaVersion<3){candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool UpdateWidgetInteractions(const StableId& id,std::vector<ScreenLocalAction> actions,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();
            auto found=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),[&](const auto& w){return w.id==id;});
            if(found==candidate.widgets.end()){error="UI interaction source is missing.";return false;}
            found->interactions=std::move(actions);
            if(candidate.schemaVersion<3){candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool UpdateWidgetComponent(const StableId& id,
            std::optional<ScreenComponentReference> link,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();
            auto found=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),[&](const auto& w){return w.id==id;});
            if(found==candidate.widgets.end()){error="Component instance is missing.";return false;}
            auto content=std::filesystem::u8path(filePath_).parent_path();
            while(!content.empty()&&content.filename()!="Content"&&content!=content.parent_path())
                content=content.parent_path();
            if(content.filename()!="Content"){error="Component requires a project Content root.";return false;}
            ScreenDocument effective;
            if(found->component&&!found->component->treeRootId.empty())
            {
                if(link)
                {
                    const auto& old=*found->component;
                    if(link->documentId!=old.documentId||link->pathHint!=old.pathHint||
                        link->widgetId!=old.widgetId||link->treeRootId!=old.treeRootId)
                    {error="Detach the full linked hierarchy before replacing a member's source.";return false;}
                    found->component=std::move(link);
                    if(!ValidateScreenDocument(candidate,projectId_,error)||
                        !ResolveScreenTheme(content.parent_path().generic_u8string(),candidate,effective,error))return false;
                    return CommitMutation(std::move(candidate),error);
                }
                if(!ResolveScreenTheme(content.parent_path().generic_u8string(),candidate,effective,error))return false;
                const auto treeRoot=found->component->treeRootId;
                for(auto& member:candidate.widgets)
                {
                    if(!member.component||member.component->treeRootId!=treeRoot)continue;
                    const auto baked=std::find_if(effective.widgets.begin(),effective.widgets.end(),
                        [&](const auto& w){return w.id==member.id;});
                    if(baked==effective.widgets.end())
                    {error="Linked hierarchy detach could not resolve an instance member.";return false;}
                    member=*baked;member.component.reset();member.themeOverrides=63;
                }
                if(!ValidateScreenDocument(candidate,projectId_,error))return false;
                return CommitMutation(std::move(candidate),error);
            }
            if(!link&&found->component)
            {
                if(!ResolveScreenTheme(content.parent_path().generic_u8string(),candidate,effective,error))return false;
                const auto baked=std::find_if(effective.widgets.begin(),effective.widgets.end(),[&](const auto& w){return w.id==id;});
                *found=*baked;found->themeOverrides=63;
            }
            found->component=std::move(link);
            if(candidate.schemaVersion<3){candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            if(!ValidateScreenDocument(candidate,projectId_,error)||
                !ResolveScreenTheme(content.parent_path().generic_u8string(),candidate,effective,error))return false;
            return CommitMutation(std::move(candidate),error);
        }

        // Explicit emergency recovery when a linked source no longer resolves.
        // Normal Detach MUST be used while the source is available so its
        // inherited presentation can be baked. This fallback keeps only the
        // last authored local values; it cannot reconstruct vanished defaults.
        [[nodiscard]] bool DetachUnavailableWidgetSource(
            const StableId& id,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();
            const auto found=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),
                [&](const auto& widget){return widget.id==id;});
            if(found==candidate.widgets.end()||!found->component)
            {error="Choose a linked UI element to recover.";return false;}
            auto content=std::filesystem::u8path(filePath_).parent_path();
            while(!content.empty()&&content.filename()!="Content"&&content!=content.parent_path())
                content=content.parent_path();
            if(content.filename()!="Content")
            {error="Linked source recovery requires a project Content root.";return false;}
            // Verify *this* linked source is the failing dependency. Another
            // missing component must not authorize silently discarding a
            // healthy source selected by the user.
            const auto treeRoot=found->component->treeRootId;
            auto sourceCheck=candidate;
            sourceCheck.theme.reset();
            for(auto& widget:sourceCheck.widgets)
            {
                if(!widget.component)continue;
                if(treeRoot.empty() ? widget.id!=id : widget.component->treeRootId!=treeRoot)
                    widget.component.reset();
            }
            ScreenDocument resolved;std::string unavailable;
            if(ResolveScreenTheme(content.parent_path().generic_u8string(),sourceCheck,resolved,unavailable))
            {error="Selected linked source is available; use normal Detach to bake inherited values.";return false;}
            for(auto& widget:candidate.widgets)
            {
                if(!widget.component)continue;
                if(treeRoot.empty() ? widget.id!=id : widget.component->treeRootId!=treeRoot)
                    continue;
                widget.component.reset();
                widget.themeOverrides=63;
            }
            // Preserve the author's last local fallback exactly. One existing
            // mutation/Undo step handles all members, never deletes widgets.
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool UpdateWidgetControl(const StableId& id,ScreenControlData control,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();
            auto w=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),[&](const auto& v){return v.id==id;});
            if(w==candidate.widgets.end()||!IsScreenValueControl(w->kind))
            {error="Selected widget is not a value control.";return false;}
            if(!ValidateScreenControl(ScreenControlKindFor(w->kind),control,error))return false;
            if(w->component)w->component->overrides|=128;
            w->control=std::move(control);
            if(candidate.schemaVersion<3){candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool UpdateWidgetAnimations(const StableId& id,
            std::vector<ScreenAnimationTrack> tracks,std::string& error)
        {
            if(!loaded_){error="No Screen is open.";return false;}
            auto candidate=Document();
            auto w=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),
                [&](const auto& value){return value.id==id;});
            if(w==candidate.widgets.end()){error="Animation target is missing.";return false;}
            if(!ValidateScreenAnimations(tracks,w->kind==ScreenWidgetKind::Progress,error))return false;
            if(w->component)w->component->overrides|=32;
            w->animations=std::move(tracks);w->themeOverrides|=32;
            if(candidate.schemaVersion<3)
            {candidate.migratedFromVersion=candidate.schemaVersion;candidate.schemaVersion=3;}
            return CommitMutation(std::move(candidate),error);
        }

        [[nodiscard]] bool UpdateWidgetFill(const StableId& widgetId, float fill,
            ScreenFillDirection direction, std::string& error)
        {
            if (!loaded_) { error = "Screen authoring session is not open."; return false; }
            auto candidate = Document();
            const auto widget = std::find_if(candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& value) { return value.id == widgetId; });
            if (widget == candidate.widgets.end() || widget->kind != ScreenWidgetKind::Progress)
            { error = "Fill editing requires a Progress element."; return false; }
            if(widget->component)widget->component->overrides|=512;
            widget->fill = fill;
            widget->fillDirection = direction;
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool UpdateProgressArtwork(const StableId& widgetId,
            std::string framePath, std::string fillPath, std::string& error)
        {
            if (!loaded_) { error = "Screen authoring session is not open."; return false; }
            auto candidate = Document();
            const auto widget = std::find_if(candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& value) { return value.id == widgetId; });
            if (widget == candidate.widgets.end() || widget->kind != ScreenWidgetKind::Progress)
            { error = "Artwork editing requires a Progress element."; return false; }
            if (widget->progressFrameResourcePath == framePath &&
                widget->progressFillResourcePath == fillPath)
            { error.clear(); return true; }
            if (widget->component) widget->component->overrides |= 512;
            widget->progressFrameResourcePath = std::move(framePath);
            widget->progressFillResourcePath = std::move(fillPath);
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool UpdateWidgetBindings(const StableId& widgetId,
            std::vector<ScreenBinding> bindings, std::string& error)
        {
            if (!loaded_) { error = "Screen authoring session is not open."; return false; }
            auto candidate = Document();
            const auto widget = std::find_if(candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& value) { return value.id == widgetId; });
            if (widget == candidate.widgets.end())
            { error = "Binding target element is missing."; return false; }
            if(widget->component)widget->component->overrides|=256;
            widget->bindings = std::move(bindings);
            if (candidate.schemaVersion < 3)
            {
                candidate.migratedFromVersion = candidate.schemaVersion;
                candidate.schemaVersion = 3;
            }
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool UpdateWidget(
            const StableId& widgetId,
            ScreenWidgetAuthoringEdit edit,
            std::string& error);

        // Direct preview manipulation updates geometry every frame.
        using GeometryCapture = std::vector<std::pair<StableId, ScreenRect>>;
        bool CaptureGeometry(const std::vector<StableId>& ids, GeometryCapture& capture,
            ScreenRect& bounds, std::string& error) const;
        bool TransformGeometry(const GeometryCapture& capture, const ScreenRect& bounds,
            const ScreenRect& target, std::string& error);
        // Coalescing keeps one complete pointer gesture as one Undo.
        void BeginCoalescedEdit()
        {
            if(coalescedEditActive_)return;
            gestureHistory_=history_;gestureIndex_=historyIndex_;gestureSavedIndex_=savedHistoryIndex_;
            coalescedEditActive_ = true;
            coalescedMutationCreated_ = false;
        }
        void CancelCoalescedEdit() noexcept
        {
            if(!coalescedEditActive_)return;
            history_=std::move(gestureHistory_);historyIndex_=gestureIndex_;savedHistoryIndex_=gestureSavedIndex_;
            ++revision_;coalescedEditActive_=false;coalescedMutationCreated_=false;
        }
        void EndCoalescedEdit() noexcept
        {
            gestureHistory_.clear();
            coalescedEditActive_ = false;
            coalescedMutationCreated_ = false;
        }

        // Complete Gate 8D presentation/binding transaction. The existing 8C
        // basic edit remains untouched; this additive path owns every remaining
        // persistent schema-v2 property and commits through the same history.
        [[nodiscard]] bool UpdateWidgetCreatorFields(
            const StableId& widgetId,
            ScreenWidgetCreatorEdit edit,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }

            ScreenDocument candidate = Document();
            const auto found = std::find_if(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& widget)
                {
                    return widget.id == widgetId;
                });
            if (found == candidate.widgets.end())
            {
                error = "Screen widget does not exist: " + widgetId;
                return false;
            }
            if (edit.parentId == widgetId)
            {
                error = "A Screen widget cannot parent itself.";
                return false;
            }

            if (!edit.parentId.empty())
            {
                StableId ancestor = edit.parentId;
                std::unordered_set<StableId> visited;
                while (!ancestor.empty())
                {
                    if (ancestor == widgetId)
                    {
                        error = "A Screen widget cannot be parented to one of its descendants.";
                        return false;
                    }
                    if (!visited.insert(ancestor).second)
                    {
                        error = "The proposed Screen parent graph already contains a cycle.";
                        return false;
                    }
                    const auto parent = std::find_if(
                        candidate.widgets.begin(), candidate.widgets.end(),
                        [&ancestor](const ScreenWidget& widget)
                        {
                            return widget.id == ancestor;
                        });
                    if (parent == candidate.widgets.end())
                    {
                        error = "The proposed Screen parent does not exist: " + ancestor;
                        return false;
                    }
                    ancestor = parent->parentId;
                }
            }

            ScreenRect resolvedBefore;
            if (!ResolveScreenWidgetRect(
                    candidate, widgetId, resolvedBefore, error))
            {
                return false;
            }

            if(found->component)
            {
                found->component->overrides|=ScreenStyleChangedGroups(found->style,edit.style);
                if(found->resourcePath!=edit.resourcePath)found->component->overrides|=8;
            }
            if(found->resourcePath!=edit.resourcePath)found->themeOverrides|=8;
            found->resourcePath = std::move(edit.resourcePath);
            found->actionId = std::move(edit.actionId);
            found->parentId = std::move(edit.parentId);
            found->layoutMode = edit.layoutMode;
            found->anchors = edit.anchors;
            found->themeOverrides|=ScreenStyleChangedGroups(found->style,edit.style);
            found->style = std::move(edit.style);

            ScreenRect parentRect{
                0.0f, 0.0f, candidate.designWidth, candidate.designHeight};
            if (!found->parentId.empty() &&
                !ResolveScreenWidgetRect(
                    candidate, found->parentId, parentRect, error))
            {
                return false;
            }
            const float relativeX = resolvedBefore.x - parentRect.x;
            const float relativeY = resolvedBefore.y - parentRect.y;
            if (found->layoutMode == ScreenLayoutMode::Absolute)
            {
                found->rect = {
                    relativeX,
                    relativeY,
                    resolvedBefore.width,
                    resolvedBefore.height,
                };
            }
            else
            {
                // Resolved preview geometry remains authoritative for anchor
                // offsets. Editing anchor min/max or changing parent therefore
                // does not make the element jump on the design canvas.
                found->anchors.offsetMinimumX = relativeX -
                    found->anchors.minimumX * parentRect.width;
                found->anchors.offsetMinimumY = relativeY -
                    found->anchors.minimumY * parentRect.height;
                found->anchors.offsetMaximumX =
                    relativeX + resolvedBefore.width -
                    found->anchors.maximumX * parentRect.width;
                found->anchors.offsetMaximumY =
                    relativeY + resolvedBefore.height -
                    found->anchors.maximumY * parentRect.height;
            }

            return CommitMutation(std::move(candidate), error);
        }

        // Symbolic Screen action identity is authored here; Story Flow remains
        // the sole authority that later routes those IDs to destinations.
        [[nodiscard]] bool AddAction(
            std::string actionId,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            if (actionId.empty())
            {
                error = "A Screen action ID cannot be empty.";
                return false;
            }
            ScreenDocument candidate = Document();
            if (candidate.actions.size() >= 64)
            {
                error = "The Screen action limit has been reached.";
                return false;
            }
            if (std::any_of(candidate.actions.begin(), candidate.actions.end(),
                    [&actionId](const ScreenAction& action)
                    {
                        return action.id == actionId;
                    }))
            {
                error = "Screen action already exists: " + actionId;
                return false;
            }
            candidate.actions.push_back({std::move(actionId)});
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool RenameAction(
            const std::string& actionId,
            std::string replacementId,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            if (replacementId.empty())
            {
                error = "A Screen action ID cannot be empty.";
                return false;
            }
            ScreenDocument candidate = Document();
            const auto found = std::find_if(
                candidate.actions.begin(), candidate.actions.end(),
                [&actionId](const ScreenAction& action)
                {
                    return action.id == actionId;
                });
            if (found == candidate.actions.end())
            {
                error = "Screen action does not exist: " + actionId;
                return false;
            }
            if (replacementId != actionId &&
                std::any_of(candidate.actions.begin(), candidate.actions.end(),
                    [&replacementId](const ScreenAction& action)
                    {
                        return action.id == replacementId;
                    }))
            {
                error = "Screen action already exists: " + replacementId;
                return false;
            }

            found->id = replacementId;
            for (auto& widget : candidate.widgets)
            {
                if (widget.kind == ScreenWidgetKind::Button &&
                    widget.actionId == actionId)
                {
                    widget.actionId = replacementId;
                }
            }
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool DeleteAction(
            const std::string& actionId,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            ScreenDocument candidate = Document();
            const auto found = std::find_if(
                candidate.actions.begin(), candidate.actions.end(),
                [&actionId](const ScreenAction& action)
                {
                    return action.id == actionId;
                });
            if (found == candidate.actions.end())
            {
                error = "Screen action does not exist: " + actionId;
                return false;
            }
            if (candidate.actions.size() <= 1)
            {
                error = "A Screen must retain at least one action.";
                return false;
            }
            if (std::any_of(candidate.widgets.begin(), candidate.widgets.end(),
                    [&actionId](const ScreenWidget& widget)
                    {
                        return widget.kind == ScreenWidgetKind::Button &&
                            widget.actionId == actionId;
                    }))
            {
                error = "Screen action is still referenced by a Button: " + actionId;
                return false;
            }
            candidate.actions.erase(found);
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool MoveButtonFocusEarlier(
            const StableId& widgetId,
            std::string& error)
        {
            return MoveButtonFocus(widgetId, -1, error);
        }

        [[nodiscard]] bool MoveButtonFocusLater(
            const StableId& widgetId,
            std::string& error)
        {
            return MoveButtonFocus(widgetId, 1, error);
        }

        // Gate 8D creator transactions. Every operation mutates a complete
        // candidate document, validates it, then enters the same bounded
        // Screen Undo/Redo history as Inspector edits.
        // Create a Button plus its *own* symbolic Screen outcome in one
        // validated mutation/Undo step. No Lua or StoryFlow graph edits.
        [[nodiscard]] bool CreateButtonWithUniqueAction(
            ScreenWidget widget,
            StableId& createdWidgetId,
            std::string& error);

        [[nodiscard]] bool CreateWidget(
            ScreenWidget widget,
            bool insertAtBack,
            StableId& createdWidgetId,
            std::string& error);
        [[nodiscard]] bool UpdateWidgetContainer(const StableId& id,
            const ScreenContainerLayout& layout, std::string& error);
        [[nodiscard]] bool CopyWidgets(const std::vector<StableId>& ids,
            ScreenWidgetClipboard& clipboard, std::string& error) const;
        [[nodiscard]] bool PasteWidgets(const ScreenWidgetClipboard& clipboard,
            float offsetX, float offsetY, std::vector<StableId>& createdRoots,
            std::string& error);
        [[nodiscard]] bool DeleteWidgets(const std::vector<StableId>& ids, std::string& error);
        [[nodiscard]] bool TranslateWidgets(const std::vector<StableId>& ids,
            float dx, float dy, std::string& error);
        [[nodiscard]] bool MoveWidgetsLayer(const std::vector<StableId>& ids,
            int direction, bool boundary, std::string& error);
        [[nodiscard]] bool MoveWidgetsAdjacentTo(const std::vector<StableId>& ids,
            const StableId& target, bool after, std::string& error);
        [[nodiscard]] bool GroupWidgets(const std::vector<StableId>& ids,
            StableId& groupId, std::string& error);
        [[nodiscard]] bool UngroupWidget(const StableId& groupId,
            std::vector<StableId>& children, std::string& error);

        [[nodiscard]] bool DuplicateWidget(
            const StableId& widgetId,
            StableId& duplicatedWidgetId,
            std::string& error);

        // Reusable components are authored Screen subtrees, not a competing
        // file format. This transaction duplicates one root plus descendants,
        // remaps every parent link and gives every copied element a fresh ID.
        [[nodiscard]] bool DuplicateWidgetTree(
            const StableId& widgetId,
            StableId& duplicatedRootId,
            std::string& error)
        {
            duplicatedRootId.clear();
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }

            ScreenDocument candidate = Document();
            const auto root = std::find_if(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& widget)
                {
                    return widget.id == widgetId;
                });
            if (root == candidate.widgets.end())
            {
                error = "Screen widget does not exist: " + widgetId;
                return false;
            }

            std::unordered_set<StableId> subtree{widgetId};
            bool expanded = true;
            while (expanded)
            {
                expanded = false;
                for (const auto& widget : candidate.widgets)
                {
                    if (!widget.parentId.empty() &&
                        subtree.count(widget.parentId) != 0 &&
                        subtree.insert(widget.id).second)
                    {
                        expanded = true;
                    }
                }
            }
            if (candidate.widgets.size() + subtree.size() > 256)
            {
                error = "Reusable component duplication would exceed the Screen element limit.";
                return false;
            }

            std::unordered_map<StableId, StableId> remap;
            remap.reserve(subtree.size());
            for (const auto& widget : candidate.widgets)
            {
                if (subtree.count(widget.id) != 0)
                    remap.emplace(widget.id, GenerateStableId());
            }

            std::vector<ScreenWidget> duplicates;
            duplicates.reserve(subtree.size());
            for (const auto& source : candidate.widgets)
            {
                const auto mapped = remap.find(source.id);
                if (mapped == remap.end()) continue;
                ScreenWidget duplicate = source;
                duplicate.id = mapped->second;
                if(duplicate.component&&!duplicate.component->treeRootId.empty())
                {
                    const auto group=remap.find(duplicate.component->treeRootId);
                    if(group==remap.end())
                    {error="Duplicate the complete linked hierarchy or detach the child first.";return false;}
                    duplicate.component->treeRootId=group->second;
                }
                if (duplicate.kind == ScreenWidgetKind::Button)
                {
                    if (candidate.actions.size() >= 64)
                    {
                        error = "Reusable component duplication exceeds the Screen action limit.";
                        return false;
                    }
                    std::string nextAction;
                    for (unsigned suffix = 1; suffix <= 64; ++suffix)
                    {
                        const auto proposed = "ui_button_" + std::to_string(suffix);
                        if (std::none_of(candidate.actions.begin(), candidate.actions.end(),
                            [&](const ScreenAction& action) { return action.id == proposed; }))
                        {
                            nextAction = proposed;
                            break;
                        }
                    }
                    if (nextAction.empty())
                    {
                        error = "No independent Button action ID is available.";
                        return false;
                    }
                    duplicate.actionId = nextAction;
                    candidate.actions.push_back({nextAction});
                }
                for(auto& action:duplicate.interactions)
                {
                    const auto target=remap.find(action.targetId);if(target!=remap.end())action.targetId=target->second;
                    if(action.condition){const auto source=remap.find(action.condition->widgetId);
                        if(source!=remap.end())action.condition->widgetId=source->second;}
                }
                if (source.id == widgetId)
                    duplicate.name += " Copy";
                const auto mappedParent = remap.find(source.parentId);
                if (mappedParent != remap.end())
                    duplicate.parentId = mappedParent->second;
                duplicates.push_back(std::move(duplicate));
            }
            candidate.widgets.insert(
                candidate.widgets.end(), duplicates.begin(), duplicates.end());

            std::vector<StableId> newFocusOrder;
            newFocusOrder.reserve(candidate.focusOrder.size() + subtree.size());
            for (const auto& focusId : candidate.focusOrder)
            {
                newFocusOrder.push_back(focusId);
                const auto mapped = remap.find(focusId);
                if (mapped != remap.end())
                    newFocusOrder.push_back(mapped->second);
            }
            candidate.focusOrder = std::move(newFocusOrder);

            const auto copiedRoot = remap.find(widgetId);
            if (copiedRoot == remap.end())
            {
                error = "Reusable component root could not be remapped.";
                return false;
            }
            const StableId newRootId = copiedRoot->second;
            if (!CommitMutation(std::move(candidate), error)) return false;
            duplicatedRootId = newRootId;
            return true;
        }

        // Import a complete source hierarchy as a linked instance. Widgets,
        // internal action targets and focus entries are remapped atomically;
        // Flow actions are independent and local geometry stays editable.
        [[nodiscard]] bool ImportLinkedWidgetTree(
            const ScreenComponentReference& sourceRoot, StableId& newRoot,
            std::string& error);
        // Add only new source descendants. Existing local overrides survive;
        // missing, reparented or changed-kind source members conflict closed.
        [[nodiscard]] bool RefreshLinkedWidgetTree(
            const StableId& instanceRoot, std::string& error);

        [[nodiscard]] bool DeleteWidget(
            const StableId& widgetId,
            std::string& error);
        [[nodiscard]] bool MoveWidgetToBack(
            const StableId& widgetId,
            std::string& error);
        [[nodiscard]] bool MoveWidgetBackward(
            const StableId& widgetId,
            std::string& error);
        [[nodiscard]] bool MoveWidgetForward(
            const StableId& widgetId,
            std::string& error);
        [[nodiscard]] bool MoveWidgetToFront(
            const StableId& widgetId,
            std::string& error);
        // Drag layer reorder: one validated, undoable sibling move.
        // Never reparents an element or changes another parent's ordering.
        [[nodiscard]] bool MoveWidgetAdjacentTo(
            const StableId& widgetId,
            const StableId& targetSiblingId,
            bool placeAfter,
            std::string& error);

        [[nodiscard]] bool Undo(std::string& error);
        [[nodiscard]] bool Redo(std::string& error);

        // Monotonic document-change token, independent of bounded Undo indices.
        // Save does not change it; mutation, history navigation, reopen and Clear do.
        [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
        [[nodiscard]] std::uint64_t HistoryMutation() const noexcept { return historyMutation_; }
        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] bool IsDirty() const noexcept;
        [[nodiscard]] bool CanUndo() const noexcept;
        [[nodiscard]] bool CanRedo() const noexcept;
        [[nodiscard]] std::size_t UndoCount() const noexcept;
        [[nodiscard]] std::size_t RedoCount() const noexcept;
        [[nodiscard]] const ScreenDocument& Document() const noexcept;
        [[nodiscard]] const ScreenWidget* FindWidget(
            const StableId& widgetId) const noexcept;
        [[nodiscard]] const std::string& FilePath() const noexcept;
        [[nodiscard]] const StableId& ProjectId() const noexcept;

    private:
        friend class ScreenFlowAtomicSaveService;
        [[nodiscard]] bool MoveWidgetLayer(
            const StableId& widgetId, int direction, bool toBoundary,
            std::string& error);

        static constexpr std::size_t InvalidHistoryIndex =
            static_cast<std::size_t>(-1);
        static constexpr std::size_t MaximumHistoryEntries = 256;

        [[nodiscard]] bool MoveButtonFocus(
            const StableId& widgetId,
            const int direction,
            std::string& error)
        {
            if (!loaded_)
            {
                error = "Screen authoring session is not open.";
                return false;
            }
            ScreenDocument candidate = Document();
            const auto widget = std::find_if(
                candidate.widgets.begin(), candidate.widgets.end(),
                [&widgetId](const ScreenWidget& value)
                {
                    return value.id == widgetId;
                });
            if (widget == candidate.widgets.end() ||
                !IsScreenInteractive(widget->kind))
            {
                error = "Screen focus order can only move interactive controls.";
                return false;
            }
            const auto focus = std::find(
                candidate.focusOrder.begin(), candidate.focusOrder.end(), widgetId);
            if (focus == candidate.focusOrder.end())
            {
                error = "The Button is missing from Screen focus order.";
                return false;
            }
            const std::ptrdiff_t index = std::distance(
                candidate.focusOrder.begin(), focus);
            const std::ptrdiff_t target = index + direction;
            if (target < 0 ||
                target >= static_cast<std::ptrdiff_t>(candidate.focusOrder.size()))
            {
                error = direction < 0
                    ? "The Button is already first in focus order."
                    : "The Button is already last in focus order.";
                return false;
            }
            std::swap(candidate.focusOrder[static_cast<std::size_t>(index)],
                candidate.focusOrder[static_cast<std::size_t>(target)]);
            return CommitMutation(std::move(candidate), error);
        }

        [[nodiscard]] bool Adopt(
            ScreenDocument document,
            const StableId& expectedProjectId,
            std::string filePath,
            std::string& error);
        [[nodiscard]] bool CommitMutation(
            ScreenDocument document,
            std::string& error);

        std::uint64_t revision_ = 0;
        std::uint64_t historyMutation_ = 0;
        ScreenDocument emptyDocument_;
        // The persisted baseline survives Undo branching and history trimming.
        ScreenDocument persistedSnapshot_;
        std::vector<std::uint8_t> persistedBytes_;
        std::vector<ScreenDocument> history_;
        std::size_t historyIndex_ = 0;
        std::size_t savedHistoryIndex_ = InvalidHistoryIndex;
        StableId projectId_;
        std::string filePath_;
        bool loaded_ = false;
        std::vector<ScreenDocument> gestureHistory_;
        std::size_t gestureIndex_=0,gestureSavedIndex_=0;
        bool coalescedEditActive_ = false;
        bool coalescedMutationCreated_ = false;
    };
}
