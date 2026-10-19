# Living Core orb, speech bubble and voice conversation

Developer implementation, 14 October 2026. No release gate closed.

## User interaction

- Click the orb to open or minimise its compact speech bubble. Drag still moves the orb.
- The default bubble is 400 x 490 pixels. Its position follows the orb on its monitor, with an edge-aware tail and screen containment. Resizing stores width/height separately from the old detached-window placement.
- Minimise retracts/hides the bubble without destroying its DOM, stopping speech or clearing chat. Reopening retains history, draft and scroll.
- Ordinary conversation uses typed questions, Daniel output and text. Existing Daniel/mute preferences remain respected. A new installation defaults to Daniel output on.
- Voice conversation requires an explicit button in the bubble or below the orb (also available in the tray menu). It is never enabled at startup or restored from settings.
- Enabled voice mode listens for an utterance and approximately 0.55 seconds without detected speech, transcribes it locally, and submits it through the same verified-first answer path as typing.
- Listening pauses while transcribing/answering/speaking. It resumes after the final audio completion. This version does not support speaking over Daniel to interrupt him; Stop voice remains separate from End voice conversation.
- End voice conversation terminates the recogniser worker and releases its microphone. It does not erase the conversation or stop a reply already playing.
- Low-confidence transcripts appear in the composer for review. With the bubble hidden, the orb button indicates Check chat. Uncertain/no-speech results do not submit guessed questions.
- Hidden chat does not disable speaking or enabled voice conversation.

## Local implementation

The approved V1 drawing and speech waveform remain intact. Only its double-click demonstration binding was removed; host click-versus-drag handling opens the bubble. Geometry retains the stable orb HWND footprint.

The sandboxed bubble has guarded minimise/conversation IPC. The orb has a sender-checked conversation IPC. Neither has editor command access. CSS handles bubble expansion/retraction and respects reduced-motion preferences.

voice/conversation.js owns opt-in, worker lifetime, recognition epochs, confidence review and stale/echo rejection. voice/conversation_worker.py uses faster-whisper base.en, CPU int8, four CPU threads, with the local model only. It opens the selected Windows input device only after a listen command. RENEGADE_STT_INPUT_DEVICE or the local ignored BUILD/assistant-stt-input.json name selects an exact input; an unavailable selection fails visibly. The microphone connection stays open during an enabled session to avoid repeated Bluetooth reconnection; callbacks discard all input while recognition or replies are active. End voice conversation closes the worker and device. Utterances are bounded to 30 seconds in RAM. Raw microphone audio is not written to disk or uploaded. The endpoint detector uses bounded pre-roll and rolling Silero speech classification through faster-whisper's bundled ONNX model. Quiet audio is normalised with bounded gain before classification and transcription; Whisper VAD also rejects nonspeech. Confidence is a log-probability heuristic, not a calibrated accuracy guarantee.

RENEGADE_STT_PYTHON and RENEGADE_STT_MODEL_DIR may override the default relative developer paths BUILD/assistant-stt-venv/Scripts/python.exe and BUILD/assistant-stt-model. Missing dependencies, input failure and model absence turn voice mode off with a visible error; typed chat remains available. No online recognition or silent alternate recogniser fallback.

## Reproducible developer setup

Use an isolated Python 3.12 environment. Do not install these dependencies into the existing Daniel or Qwen environments.

    uv venv --python 3.12 BUILD/assistant-stt-venv
    uv pip install --python BUILD/assistant-stt-venv/Scripts/python.exe -r Tools/assistant_presence/voice/requirements-stt.txt
    BUILD/assistant-stt-venv/Scripts/python.exe Tools/assistant_presence/voice/download_stt_model.py
    BUILD/assistant-stt-venv/Scripts/python.exe Tools/assistant_presence/voice/conversation_worker.py --probe
    BUILD/assistant-stt-venv/Scripts/python.exe Tools/assistant_presence/tests/test_turn_detector.py
    node Tools/assistant_presence/tests/test-conversation.cjs
    node Tools/assistant_presence/tests/test-main.cjs

Downloading is an explicit setup step, never done by voice mode. The approximately 145 MB model and Python environment currently live under ignored BUILD. Distribution, licences/notices and packaged path ownership remain part of the future packaging milestone.

Direct dependency versions are pinned. av 19.0.1 was incompatible with faster-whisper 1.2.1's metadata_errors input argument; av 16.1.0 passes the actual WAV proof. Windows System.Speech was tried but rejected: it misheard the actual editor question at low confidence. That experimental backend is retained only under ignored BUILD, not as an app fallback.

Primary API references: https://github.com/SYSTRAN/faster-whisper and https://python-sounddevice.readthedocs.io/en/latest/.

## Verification and remaining acceptance

Automated controller tests cover explicit opt-in, no mic on typing, accepted turn, playback pause/resume, minimise independence, End voice worker cleanup, low-confidence review, stale results/old workers, failure cleanup and multi-monitor containment. Endpoint tests cover silence, short noise, pauses, bounded utterances and cancellation reset.

Real Electron visual/client proof verifies compact geometry, actual verified answer, microphone off, history/scroll retention and speaking while minimised. Real recorded-audio proof verifies offline transcription into the existing Character workflow. V1 waveform, streaming speech, formatting and verified-knowledge regressions pass.

Human microphone acceptance remains open: test the actual input device, conversational speech, quiet voice, accent, background noise, speaker echo at turn boundaries, several successive turns, minimised listening, uncertain transcript correction and End voice. No acoustic echo cancellation, barge-in, microphone chooser or per-device threshold calibration is claimed. Real microphone hardware was not activated automatically during development.

## Microphone reliability repair, 14 October 2026

Owner rejected live voice acceptance: approximately one of ten sentences recognised, excessive delay and silent Daniel following an incorrect direct Electron launch. These behavioural failures override earlier recorded-audio proof. No acceptance or release completion claimed.

Repair keeps the explicitly enabled Bluetooth input connection stable, replaces loudness-only endpoint decisions with speech classification, normalises quiet audio, exposes hearing/transcribing states and recognition duration, prevents transient listening during answer startup, and routes narrow microphone checks to a truthful immediate acknowledgement. Daniel still uses the existing launcher/environment. Regression and three-volume recorded-audio checks pass; fresh owner repeated-turn acceptance remains required.
