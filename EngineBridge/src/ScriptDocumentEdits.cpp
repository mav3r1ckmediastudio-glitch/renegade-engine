#include "renegade/bridge/ScriptDocumentService.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

namespace
{
    using renegade::bridge::ICommand;
    using renegade::bridge::ScriptAttachment;
    using renegade::bridge::ScriptDocument;
    using renegade::bridge::ScriptPropertyValue;
    using renegade::bridge::ScriptScope;
    using renegade::bridge::StableId;

    std::string OrderGroup(const ScriptAttachment& attachment)
    {
        return std::to_string(static_cast<int>(attachment.scope)) + "|" +
            attachment.ownerEntityId;
    }

    bool CommitCandidate(
        ScriptDocument& document,
        ScriptDocument candidate,
        std::string& error)
    {
        renegade::bridge::NormalizeScriptAttachmentOrder(candidate);
        if (!renegade::bridge::ValidateScriptDocument(candidate, error))
            return false;
        document = std::move(candidate);
        error.clear();
        return true;
    }

    class ScriptDocumentMutationCommand final : public ICommand
    {
    public:
        ScriptDocumentMutationCommand(
            ScriptDocument& document,
            ScriptDocument before,
            ScriptDocument after)
            : document_(&document)
            , before_(std::move(before))
            , after_(std::move(after))
        {
        }

        bool Execute() override
        {
            if (document_ == nullptr)
                return false;
            *document_ = after_;
            return true;
        }

        void Undo() override
        {
            if (document_ != nullptr)
                *document_ = before_;
        }

    private:
        ScriptDocument* document_ = nullptr;
        ScriptDocument before_;
        ScriptDocument after_;
    };

    template<typename Mutation>
    std::unique_ptr<ICommand> MakeMutationCommand(
        ScriptDocument& document,
        Mutation&& mutation,
        std::string& error)
    {
        ScriptDocument after = document;
        if (!mutation(after, error))
            return {};
        return std::make_unique<ScriptDocumentMutationCommand>(
            document,
            document,
            std::move(after));
    }
}

namespace renegade::bridge
{
    void NormalizeScriptAttachmentOrder(ScriptDocument& document)
    {
        std::unordered_map<std::string, std::vector<ScriptAttachment*>> groups;
        for (auto& attachment : document.attachments)
            groups[OrderGroup(attachment)].push_back(&attachment);

        for (auto& item : groups)
        {
            auto& group = item.second;
            std::stable_sort(
                group.begin(), group.end(),
                [](const ScriptAttachment* left, const ScriptAttachment* right)
                {
                    return left->order < right->order;
                });
            for (std::size_t index = 0; index < group.size(); ++index)
                group[index]->order = static_cast<std::uint32_t>(index);
        }
    }

    bool AddScriptAttachment(
        ScriptDocument& document,
        ScriptAttachment attachment,
        std::string& error)
    {
        ScriptDocument candidate = document;
        if (attachment.scriptInstanceId.empty())
            attachment.scriptInstanceId = GenerateStableId();
        if (attachment.sourceId.empty())
            attachment.sourceId = GenerateStableId();

        std::uint32_t endOrder = 0;
        for (const auto& existing : candidate.attachments)
        {
            if (existing.scope == attachment.scope &&
                existing.ownerEntityId == attachment.ownerEntityId)
            {
                endOrder = std::max(endOrder, existing.order + 1);
            }
        }
        attachment.order = endOrder;
        candidate.attachments.push_back(std::move(attachment));
        return CommitCandidate(document, std::move(candidate), error);
    }

    bool RemoveScriptAttachment(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::string& error)
    {
        ScriptDocument candidate = document;
        const auto found = std::find_if(
            candidate.attachments.begin(), candidate.attachments.end(),
            [&](const ScriptAttachment& attachment)
            {
                return attachment.scriptInstanceId == scriptInstanceId;
            });
        if (found == candidate.attachments.end())
        {
            error = "ScriptInstanceId was not attached to this document.";
            return false;
        }
        candidate.attachments.erase(found);
        return CommitCandidate(document, std::move(candidate), error);
    }

    bool SetScriptAttachmentEnabled(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const bool enabled,
        std::string& error)
    {
        ScriptDocument candidate = document;
        auto* attachment = FindScriptAttachment(candidate, scriptInstanceId);
        if (attachment == nullptr)
        {
            error = "ScriptInstanceId was not attached to this document.";
            return false;
        }
        if (attachment->enabled == enabled)
        {
            error = "Script enabled state is already the requested value.";
            return false;
        }
        attachment->enabled = enabled;
        return CommitCandidate(document, std::move(candidate), error);
    }

    bool MoveScriptAttachment(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const std::uint32_t newOrder,
        std::string& error)
    {
        ScriptDocument candidate = document;
        auto* target = FindScriptAttachment(candidate, scriptInstanceId);
        if (target == nullptr)
        {
            error = "ScriptInstanceId was not attached to this document.";
            return false;
        }

        std::vector<ScriptAttachment*> group;
        for (auto& attachment : candidate.attachments)
        {
            if (attachment.scope == target->scope &&
                attachment.ownerEntityId == target->ownerEntityId)
            {
                group.push_back(&attachment);
            }
        }
        std::stable_sort(
            group.begin(), group.end(),
            [](const ScriptAttachment* left, const ScriptAttachment* right)
            {
                return left->order < right->order;
            });

        const auto found = std::find(group.begin(), group.end(), target);
        if (found == group.end())
        {
            error = "Could not resolve script order group.";
            return false;
        }
        const std::size_t oldIndex = static_cast<std::size_t>(
            std::distance(group.begin(), found));
        const std::size_t requested = group.empty()
            ? 0
            : std::min<std::size_t>(newOrder, group.size() - 1);
        if (oldIndex == requested)
        {
            error = "Script attachment is already at the requested order.";
            return false;
        }

        ScriptAttachment* moved = *found;
        group.erase(found);
        group.insert(group.begin() + static_cast<std::ptrdiff_t>(requested), moved);
        for (std::size_t index = 0; index < group.size(); ++index)
            group[index]->order = static_cast<std::uint32_t>(index);

        return CommitCandidate(document, std::move(candidate), error);
    }

    bool ReplaceScriptSourceBinding(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const ScriptSourceBinding& source,
        std::string& error)
    {
        ScriptDocument candidate = document;
        auto* attachment = FindScriptAttachment(candidate, scriptInstanceId);
        if (attachment == nullptr)
        {
            error = "ScriptInstanceId was not attached to this document.";
            return false;
        }
        if (!IsValidStableId(source.sourceId))
        {
            error = "Replacement script source requires a valid sourceId.";
            return false;
        }

        attachment->sourceId = source.sourceId;
        attachment->sourcePath = source.sourcePath;
        attachment->presentation = source.presentation;
        attachment->apiVersion = source.apiVersion;
        attachment->unsafe = source.unsafe;
        attachment->provenance = source.provenance;
        attachment->dependencies = source.dependencies;
        attachment->capabilities = source.capabilities;
        return CommitCandidate(document, std::move(candidate), error);
    }

    bool SetScriptProperty(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        ScriptPropertyValue property,
        std::string& error)
    {
        ScriptDocument candidate = document;
        auto* attachment = FindScriptAttachment(candidate, scriptInstanceId);
        if (attachment == nullptr)
        {
            error = "ScriptInstanceId was not attached to this document.";
            return false;
        }
        const auto existing = std::find_if(
            attachment->properties.begin(), attachment->properties.end(),
            [&](const ScriptPropertyValue& value)
            {
                return value.name == property.name;
            });
        if (existing == attachment->properties.end())
            attachment->properties.push_back(std::move(property));
        else
            *existing = std::move(property);
        return CommitCandidate(document, std::move(candidate), error);
    }

    bool RemoveScriptProperty(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const std::string& propertyName,
        std::string& error)
    {
        ScriptDocument candidate = document;
        auto* attachment = FindScriptAttachment(candidate, scriptInstanceId);
        if (attachment == nullptr)
        {
            error = "ScriptInstanceId was not attached to this document.";
            return false;
        }
        const auto existing = std::find_if(
            attachment->properties.begin(), attachment->properties.end(),
            [&](const ScriptPropertyValue& value)
            {
                return value.name == propertyName;
            });
        if (existing == attachment->properties.end())
        {
            error = "Script property was not present on the attachment.";
            return false;
        }
        attachment->properties.erase(existing);
        return CommitCandidate(document, std::move(candidate), error);
    }

    bool DuplicateScriptInstance(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        StableId& duplicateScriptInstanceId,
        std::string& error)
    {
        ScriptDocument candidate = document;
        const auto* source = FindScriptAttachment(candidate, scriptInstanceId);
        if (source == nullptr)
        {
            error = "ScriptInstanceId was not attached to this document.";
            return false;
        }

        ScriptAttachment duplicate = *source;
        duplicate.scriptInstanceId = GenerateStableId();
        std::uint32_t endOrder = 0;
        for (const auto& attachment : candidate.attachments)
        {
            if (attachment.scope == duplicate.scope &&
                attachment.ownerEntityId == duplicate.ownerEntityId)
            {
                endOrder = std::max(endOrder, attachment.order + 1);
            }
        }
        duplicate.order = endOrder;
        duplicateScriptInstanceId = duplicate.scriptInstanceId;
        candidate.attachments.push_back(std::move(duplicate));
        return CommitCandidate(document, std::move(candidate), error);
    }

    bool DuplicateEntityScriptAttachments(
        ScriptDocument& document,
        const StableId& sourceOwnerEntityId,
        const StableId& duplicateOwnerEntityId,
        std::vector<StableId>& duplicateScriptInstanceIds,
        std::string& error)
    {
        if (!IsValidStableId(sourceOwnerEntityId) ||
            !IsValidStableId(duplicateOwnerEntityId) ||
            sourceOwnerEntityId == duplicateOwnerEntityId)
        {
            error = "Entity script duplication requires two distinct valid persistent entity IDs.";
            return false;
        }

        std::vector<ScriptAttachment> sourceAttachments;
        for (const auto& attachment : document.attachments)
        {
            if (attachment.scope == ScriptScope::Entity &&
                attachment.ownerEntityId == sourceOwnerEntityId)
            {
                sourceAttachments.push_back(attachment);
            }
        }
        if (sourceAttachments.empty())
        {
            error = "Source entity has no script attachments to duplicate.";
            return false;
        }
        std::sort(
            sourceAttachments.begin(), sourceAttachments.end(),
            [](const ScriptAttachment& left, const ScriptAttachment& right)
            {
                return left.order < right.order;
            });

        ScriptDocument candidate = document;
        std::uint32_t nextOrder = 0;
        for (const auto& attachment : candidate.attachments)
        {
            if (attachment.scope == ScriptScope::Entity &&
                attachment.ownerEntityId == duplicateOwnerEntityId)
            {
                nextOrder = std::max(nextOrder, attachment.order + 1);
            }
        }

        std::vector<StableId> generated;
        generated.reserve(sourceAttachments.size());
        for (auto attachment : sourceAttachments)
        {
            attachment.scriptInstanceId = GenerateStableId();
            attachment.ownerEntityId = duplicateOwnerEntityId;
            attachment.order = nextOrder++;
            generated.push_back(attachment.scriptInstanceId);
            candidate.attachments.push_back(std::move(attachment));
        }

        if (!CommitCandidate(document, std::move(candidate), error))
            return false;
        duplicateScriptInstanceIds = std::move(generated);
        error.clear();
        return true;
    }

    std::unique_ptr<ICommand> MakeAddScriptAttachmentCommand(
        ScriptDocument& document,
        ScriptAttachment attachment,
        std::string& error)
    {
        return MakeMutationCommand(
            document,
            [attachment = std::move(attachment)](
                ScriptDocument& candidate, std::string& mutationError) mutable
            {
                return AddScriptAttachment(
                    candidate, std::move(attachment), mutationError);
            },
            error);
    }

    std::unique_ptr<ICommand> MakeRemoveScriptAttachmentCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::string& error)
    {
        return MakeMutationCommand(
            document,
            [scriptInstanceId](ScriptDocument& candidate, std::string& mutationError)
            {
                return RemoveScriptAttachment(candidate, scriptInstanceId, mutationError);
            },
            error);
    }

    std::unique_ptr<ICommand> MakeSetScriptEnabledCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const bool enabled,
        std::string& error)
    {
        return MakeMutationCommand(
            document,
            [scriptInstanceId, enabled](ScriptDocument& candidate, std::string& mutationError)
            {
                return SetScriptAttachmentEnabled(
                    candidate, scriptInstanceId, enabled, mutationError);
            },
            error);
    }

    std::unique_ptr<ICommand> MakeMoveScriptAttachmentCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        const std::uint32_t newOrder,
        std::string& error)
    {
        return MakeMutationCommand(
            document,
            [scriptInstanceId, newOrder](ScriptDocument& candidate, std::string& mutationError)
            {
                return MoveScriptAttachment(
                    candidate, scriptInstanceId, newOrder, mutationError);
            },
            error);
    }

    std::unique_ptr<ICommand> MakeReplaceScriptSourceCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        ScriptSourceBinding source,
        std::string& error)
    {
        return MakeMutationCommand(
            document,
            [scriptInstanceId, source = std::move(source)](
                ScriptDocument& candidate, std::string& mutationError)
            {
                return ReplaceScriptSourceBinding(
                    candidate, scriptInstanceId, source, mutationError);
            },
            error);
    }

    std::unique_ptr<ICommand> MakeSetScriptPropertyCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        ScriptPropertyValue property,
        std::string& error)
    {
        return MakeMutationCommand(
            document,
            [scriptInstanceId, property = std::move(property)](
                ScriptDocument& candidate, std::string& mutationError) mutable
            {
                return SetScriptProperty(
                    candidate, scriptInstanceId, std::move(property), mutationError);
            },
            error);
    }

    std::unique_ptr<ICommand> MakeRemoveScriptPropertyCommand(
        ScriptDocument& document,
        const StableId& scriptInstanceId,
        std::string propertyName,
        std::string& error)
    {
        return MakeMutationCommand(
            document,
            [scriptInstanceId, propertyName = std::move(propertyName)](
                ScriptDocument& candidate, std::string& mutationError)
            {
                return RemoveScriptProperty(
                    candidate, scriptInstanceId, propertyName, mutationError);
            },
            error);
    }
}
