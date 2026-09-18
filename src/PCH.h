#pragma once

// CommonLibSSE-NG umbrella headers (also pulls in <Windows.h>, spdlog, etc.).
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>

// <windows.h> arrives through spdlog's file sink, not through the library
// (CommonLibSSE-NG keeps Win32 behind REX::W32), and its GDI macro would
// rename InventoryEntryData::GetObject to GetObjectW in every file below.
#ifdef GetObject
#    undef GetObject
#endif

// Matches the CommonLibSSE-NG template convention (""sv literals in the
// auto-generated plugin declaration).
using namespace std::literals;
