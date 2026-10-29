// Legacy Rogue - TUN-4 diagnostic probe (read-only). See src/rogue_probe.cpp.
// Gated on NHL_ROGUE=1 AND NHL_ROGUE_PROBE=1. Driven from the panel's Diagnostics section.
#pragma once

namespace rogue::probe {
bool Enabled();                   // both env vars set
bool Busy();                      // a snapshot is running on the worker thread
void RunAsync(const char* label); // append one snapshot to rogue_probe.txt (next to the exe)
const char* Status();             // one-line status for the panel
}  // namespace rogue::probe
