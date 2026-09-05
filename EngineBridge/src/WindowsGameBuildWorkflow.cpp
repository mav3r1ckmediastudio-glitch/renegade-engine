#include "renegade/bridge/WindowsGameBuildWorkflow.h"

namespace renegade::bridge
{
    bool BuildWindowsGame(
        const WindowsGameBuildWorkflowRequest& request,
        const WindowsGameBuildSmokeRunner& smokeRunner,
        WindowsGameBuildWorkflowResult& result,
        std::string& error)
    {
        result = {};
        error.clear();

        if (!smokeRunner)
        {
            error = "Build Windows Game requires a staged Runtime smoke runner.";
            return false;
        }

        if (!CreateWindowsGameBuildPlan(
                request.project,
                request.dependencyGraph,
                request.assetRegistry,
                request.build,
                request.runtimeSupport,
                result.plan,
                error))
        {
            if (error.empty()) error = "Build Windows Game failed while creating the build plan.";
            return false;
        }

        if (!StageWindowsGameBuild(
                result.plan,
                request.staging,
                result.stage,
                error))
        {
            if (error.empty()) error = "Build Windows Game failed while staging the package.";
            return false;
        }

        if (!ApplyWindowsGameExecutableIdentity(
                result.plan,
                request.identity,
                result.stage,
                result.identity,
                error))
        {
            if (error.empty()) error = "Build Windows Game failed while applying executable identity.";
            return false;
        }

        if (!smokeRunner(
                result.plan,
                result.stage,
                result.runtimeEvidencePath,
                error))
        {
            if (error.empty()) error = "Build Windows Game staged Runtime smoke failed without a diagnostic.";
            return false;
        }
        if (result.runtimeEvidencePath.empty())
        {
            error = "Build Windows Game smoke completed without Runtime evidence.";
            return false;
        }

        WindowsGameBuildVerificationRequest verification = request.verification;
        verification.packageRootPath = result.stage.stagingPath;
        verification.runtimeEvidencePath = result.runtimeEvidencePath;
        if (!RecordWindowsGameBuildVerification(
                verification,
                result.verification,
                error))
        {
            if (error.empty()) error = "Build Windows Game failed while recording verification evidence.";
            return false;
        }

        WindowsGameBuildPromotionRequest promotion;
        promotion.candidatePath = result.stage.stagingPath;
        promotion.finalOutputPath = result.stage.finalOutputPath;
        if (!PromoteWindowsGameBuild(
                promotion,
                result.promotion,
                error))
        {
            if (error.empty()) error = "Build Windows Game failed while promoting the staged package.";
            return false;
        }

        if (!result.promotion.succeeded ||
            result.promotion.finalOutputPath.empty())
        {
            error = "Build Windows Game promotion returned no accepted final build.";
            return false;
        }

        result.finalOutputPath = result.promotion.finalOutputPath;
        error.clear();
        return true;
    }
}
