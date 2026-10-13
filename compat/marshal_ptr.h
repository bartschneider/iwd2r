// 32-bit handles for pointers the original stores in 32-bit file fields.
//
// When the original marshals an area, the door, container, creature and
// trigger records carry heap pointers in fields the file format gives 32
// bits, and the game-file writers read those fields back as pointers before
// the save is written out. On a 64-bit build a pointer does not fit, so the
// producers store a handle from MarshalPtr and the consumers resolve it with
// UnmarshalPtr. The file contents of those fields were never meaningful
// after a reload (the original overwrites them on load), so the handle
// values are fine on every platform. Call ResetMarshalPtrs at the start of
// each marshal pass (SaveGame) so the table does not grow for the run.
#pragma once

#include <cstdint>

namespace iwd2r {

// 0 for nullptr; the same pointer always gets the same handle within a pass.
uint32_t MarshalPtr(const void* p);

// nullptr for 0 and for a handle this pass never issued.
void* UnmarshalPtr(uint32_t handle);

void ResetMarshalPtrs();

} // namespace iwd2r
