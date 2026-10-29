// Legacy Rogue - TUN-4 diagnostic probe (read-only) + TUN-5 write test. See src/rogue_probe.cpp.
// Gated on NHL_ROGUE=1 AND NHL_ROGUE_PROBE=1. Driven from the panel's Diagnostics section.
#pragma once

namespace rogue::probe {
bool Enabled();                   // both env vars set
bool Busy();                      // a snapshot is running on the worker thread
void RunAsync(const char* label); // append one snapshot to rogue_probe.txt (next to the exe)
const char* Status();             // one-line status for the panel

// TUN-5b write test: writes the field real code reads (N+32 + heap copy) for floats/ints, or
// every entry of a game-style table. 3 presets per setting. Restart or Restore to undo.
int WriteTestCount();
const char* WriteTestName(int i);
const char* WriteTestPresetLabel(int i, int p);  // p = 0..2
bool WriteTestApply(int i, int p);
void WriteTestRestore(int i);
void WriteTestRestoreAll();
void WriteTestDescribe(int i, char* buf, unsigned n);
// TUN-9: write rogue_tweak_catalog.tsv (every tweak object, live value at N+32). Async.
void CatalogAsync();
}  // namespace rogue::probe
