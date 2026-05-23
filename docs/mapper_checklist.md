# Mapper Implementation Checklist

When adding or debugging a mapper in ear6, verify every item below.
Each item corresponds to a real bug found in production.

---

## 1. Init: Mirroring MUST Be Set

**Check:** `init()` calls `set_mirroring_type(info.mirroring)`.

**Why:** `BaseMapper`'s default `mirroring_type_` is `HORIZONTAL`, but the
default only sets the enum — it does **not** call `set_nametable()` to
wire up the PPU memory mapping (`chr_pages_[0x20..0x23]`). Without an
explicit `set_mirroring_type()` call, nametable VRAM reads and writes go
through unmapped (zero-initialized) pages, causing:

- CPU writes to nametable via `$2007` silently lost
- PPU reads return address low byte instead of written data
- First rendered frame shows completely wrong tiles/colors
- Frames before rendering is enabled may match mesen2 (both in forced
  blank), diverging only on the first rendered frame

**Bug history:** Mapper 006 `init()` omitted `set_mirroring_type()`.
Argus (J).nes (mapper 6) showed pixel-perfect match for frames 1-2
(forced blank) but 44378 differing pixels in frame 3 (first rendered
frame). Root cause: `write_vram($209E, $30)` wrote to an unmapped page;
`read_vram($209E)` returned $9E (address low byte from stale page).

**Reference mappers that do it correctly:**
- Mapper 000: `set_mirroring_type(info.mirroring)` in `init()`
- Mapper 002: same
- Mapper 009: same

**Fix pattern:**
```cpp
void MapperXXX::init(const RomInfo& info, ...) {
    // ... other init ...
    set_mirroring_type(info.mirroring);   // <-- MANDATORY
    // ... register ranges, PRG/CHR mapping ...
}
```

---

## 2. Init: PRG/CHR Page Mapping

**Check:** `init()` maps all PRG and CHR windows using the correct API.

**Known pitfall:** `select_prg_page_2x(slot, page)` has a guard
`if (get_prg_page_size() < 0x2000)` that silently maps only 1 slot
instead of 2 when `page_size == 0x2000`. This differs from mesen2's
`SelectPrgPage2x` which always maps both slots.

**Fix pattern:** Use individual `select_prg_page()` calls:
```cpp
// Instead of:
select_prg_page_2x(0, page);

// Use:
select_prg_page(0, page);
select_prg_page(1, page + 1);
```

**Bug history:** Mapper 006 init used `select_prg_page_2x(0, 0)` +
`select_prg_page_2x(1, 14)`, leaving $C000-$FFFF unmapped. CPU read
reset vector from $0000 → executed BRK loop → single-color output.

---

## 3. Write Register: PRG Banking Condition

**Check:** PRG banking in `write_register()` uses the same condition as
mesen2.

**Bug history:** Mapper 006 PRG banking condition was inverted
(`!ffe_alt_mode_` instead of `has_chr_ram() || ffe_alt_mode_`), causing
PRG bank switching at wrong times.

---

## 4. Write Register: CHR Banking

**Check:** CHR bank selection uses the correct page size API.

**Common issue:** `select_chr_page_8x()` delegates to two
`select_chr_page_4x()` calls. Verify the slot and page arithmetic
matches mesen2.

---

## 5. Mirroring: Dynamic Switching

**Check:** If the mapper supports runtime mirroring changes (e.g. via
register writes like $42FE/$42FF in mapper 6), the `write_register()`
handler calls `set_mirroring_type()` with the correct type.

---

## 6. IRQ: Counter and Enable

**Check:** IRQ counter width, enable/disable logic, and
`process_cpu_clock()` match mesen2 exactly.

---

## 7. Save State: Serialize All Custom State

**Check:** `Serialize()` covers every custom member variable
(irq_counter, irq_enabled, ffe_alt_mode, etc.).

---

## Debugging Workflow for Mapper Mismatches

1. **Screenshot diff per frame:** Run both emulators for frames 1..N,
   compare PPM MD5. Find the first diverging frame.

2. **Per-cycle PPU trace:** Add `EAR6_TRACE_TILE_INDEX` /
   `MESEN2_TILE_TRACE` to both emulators. Diff to find the first cycle
   where `idx` differs at the same `nt` address.

3. **Classify the divergence:**
   - **Same address, different data** → VRAM content wrong (mirroring,
     write lost, write redirected)
   - **Different address** → `v` register / scroll setup wrong
   - **Same data, different palette** → attribute or palette RAM issue

4. **Trace writes to the diverging address:** Search `$2007` write log
   for the target VRAM address. Verify `ppu_bus_address_` matches
   `video_ram_addr_` at write time. Verify `write_vram` actually
   stores the value (check `chr_memory_access_` has WRITE bit).

5. **If write is lost:** Check that `set_mirroring_type()` was called
   during `init()`. Without it, nametable pages are unmapped and
   `write_vram` is a no-op.
