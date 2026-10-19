param(
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [Parameter(Mandatory=$true)][ValidateSet('Microsoft Hazel Desktop','Microsoft David Desktop')][string]$VoiceName
)
$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Speech
    $text = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($text) -or $text.Length -gt 2000) { throw 'Invalid speech text' }
    $speech = New-Object System.Speech.Synthesis.SpeechSynthesizer
    try {
        $speech.SelectVoice($VoiceName)
        $speech.Rate = -1
        $speech.Volume = 100
        $speech.SetOutputToWaveFile($OutputPath)
        $speech.Speak($text)
        $speech.SetOutputToNull()
    } finally { $speech.Dispose() }
    if (-not (Test-Path $OutputPath) -or (Get-Item $OutputPath).Length -lt 100) { throw 'No WAV file was generated' }
    exit 0
} catch {
    [Console]::Error.WriteLine("Windows voice failed: " + $_.Exception.Message)
    exit 1
}
