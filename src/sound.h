#pragma once
// Plays the "mic muted" or "mic unmuted" sound. Both sounds are WAV files embedded in the exe
// as resources (assets/sounds/hollow_on.wav, hollow_off.wav), so no extra files are needed at runtime.
void PlayMuteSound(bool muted);
