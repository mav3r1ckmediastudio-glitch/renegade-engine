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
            return false;
        }

        if (!StageWindowsGameBuild(
                result.plan,
                request.staging,
                result.stage,
                error))
        {
            return false;
        }

        if (!ApplyWindowsGameExecutableIdentity(
                result.plan,
                request.identity,
                result.stage,
                result.identity,
                error))
        {
            return false;
        }

        if (!smokeRunner(
                result.plan,
                result.stage,
                result.runtimeEvidencePath,
                error))
        {
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
