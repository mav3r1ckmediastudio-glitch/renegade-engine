$ErrorActionPreference='Stop'
$env:GIT_CONFIG_COUNT='3'
$env:GIT_CONFIG_KEY_0='safe.directory';$env:GIT_CONFIG_VALUE_0='E:/RenegadeUI-20261116'
$env:GIT_CONFIG_KEY_1='safe.directory';$env:GIT_CONFIG_VALUE_1='E:/RenegadeUI-20261116/WickedEngine'
$env:GIT_CONFIG_KEY_2='safe.directory';$env:GIT_CONFIG_VALUE_2='E:/RenegadeUI-20261116/ThirdParty/imnodes'
& ctest --test-dir E:/RenegadeUI-20261116/BUILD/native -C Release -R 'SourceContract|Assistant|StudioUiPolicy' --output-on-failure *> E:/RenegadeUI-20261116/BUILD/ui197-uma-recovery/source-help-policy.log
$code=$LASTEXITCODE
Get-Content E:/RenegadeUI-20261116/BUILD/ui197-uma-recovery/source-help-policy.log -Tail 20
if($code -ne 0){throw 'Source/help/policy checks failed'}
Write-Output 'SOURCE_HELP_POLICY_PASS'
