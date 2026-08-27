// The upgrade sequence itself.
//
// Everything here follows from facts read in the firmware, not from guesses:
//
//  * the old bootloader's service loop runs
//        while( stopwatch < 20000 || BSL_SYSEX_HaltStateGet() || hold_pin )
//    (D:/MIOS32/trunk/bootloader/src/main.c:383) - so the 2 s window is only a
//    STARTING delay: the first successful flash write sets the halt state and
//    holds the bootloader for as long as we keep writing.
//
//  * that bootloader refuses writes below its own boundary, and jumps BLINDLY
//    to it when the window closes. It has no entry-override command (0x03 was
//    added in August 2026), so a tool linked above the boundary is reached the
//    way ADIOS Studio reaches it on a legacy core: by copying the tool's first
//    256 bytes - its vector table, whose addresses are absolute and point into
//    the real code - TO the boundary, as a signpost the blind jump lands on.
//
//  * the upgrade image carries TWO stages in disjoint ranges: the tool above
//    the boundary, the new bootloader at 0x08000000. Stage 1 goes to the old
//    bootloader; stage 2 goes to the TOOL once it is running, because the old
//    bootloader would refuse those addresses outright.

#include "../core/sysex.h"
#include "../core/hexfile.h"
#include "../platform/midi_win.h"
#include "upgrade.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>
#include <cstdarg>
#include <string>

namespace tr5x6 {

// printf-style plumbing for the two report calls.
void Log::info(const char* fmt, ...)
{
	char buf[512];
	va_list ap; va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
	if (infoFn) infoFn(buf);
}

void Log::err(const char* fmt, ...)
{
	char buf[512];
	va_list ap; va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
	if (errFn) errFn(buf);
}

void Log::warn(const char* fmt, ...)
{
	char buf[512];
	va_list ap; va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
	if (warnFn) warnFn(buf); else if (infoFn) infoFn(buf);
}

void Log::ok(const char* fmt, ...)
{
	char buf[512];
	va_list ap; va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
	if (okFn) okFn(buf); else if (infoFn) infoFn(buf);
}

void Log::status(const char* fmt, ...)
{
	char buf[512];
	va_list ap; va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
	if (statusFn) statusFn(buf);
}

namespace {

const size_t BLOCK = 256;

// Waits for query 0x0b to answer a given core type. Measured on a real board
// on 2026-08-27: a bare restart takes 3.3 s end to end (2 s of bootloader
// window, then the application booting its display). Fixed sleeps sized on a
// guess were shorter than that - polling for WHO ANSWERS is both quicker when
// things go well and honest when they do not.
bool waitForCore(Link& lk, const char* want, int seconds, Log& log)
{
    // TIME-based, not attempt-based: a board that refuses the query answers
    // instantly, so counting attempts burned 50 tries in about a second and
    // called it a ten-second wait.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        Reply r = lk.exchange(query(lk.deviceId, 0x0b), 180);
        if (r.valid && r.cmd == ACK && r.text() == want) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
    log.err("no %s answered within %d s", want, seconds);
    return false;
}

// After the jump, WHICH device id does the tool answer on? Its own record is
// not at the new boundary yet, so ADIOS_MIDI_Init read an address that still
// holds old bootloader code - the id it ends up with is whatever that byte
// says. The tool rescues the real one from the scanned info block, but only
// when that block carries a device-id marker, and the field boards have none.
// So: find it rather than assume it. Sixteen short queries cost nothing.
bool findTool(Link& lk, int seconds, Log& log)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        for (uint8_t id = 0; id < 16; ++id) {
            const uint8_t saved = lk.deviceId;
            lk.deviceId = id;
            Reply r = lk.exchange(query(id, 0x01), 120);   // OS name
            if (r.valid && r.cmd == ACK && r.text() == "ADIOS") {
                if (id != saved) log.info("the tool answers on device id %u", id);
                return true;
            }
            lk.deviceId = saved;
        }
    }
    return false;
}

// One write-memory exchange, padded to the 16-byte granularity the address
// encoding imposes.
Reply writeBlock(Link& lk, uint32_t addr, const uint8_t* p, size_t n, int timeoutMs)
{
    std::vector<uint8_t> chunk(p, p + n);
    while (chunk.size() % 16) chunk.push_back(0xff);
    return lk.exchange(writeMem(lk.deviceId, addr, chunk.data(), chunk.size()), timeoutMs);
}

// Flattens a set of segments into ONE contiguous run, gaps filled with 0xff.
// Sending segment by segment and padding each to the 16-byte granularity made
// the padding of one overlap the start of the next: on G0 a double-word that
// is already programmed cannot be programmed again without an erase, and the
// board answered WRITE_FAILED. Measured 2026-08-27 at 0x08001BBC, six bytes
// inside the previous block padding.
// forceLo/forceHi impose a span instead of using the segments own extent -
// used for the BSL region, which is written IN FULL so that no remnant of a
// half-finished attempt can survive underneath the new bootloader.
HexSegment flatten(const std::vector<const HexSegment*>& segs,
                   uint32_t forceLo = 0, uint32_t forceHi = 0)
{
    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    for (auto* s : segs) {
        if (s->addr < lo) lo = s->addr;
        const uint32_t end = s->addr + static_cast<uint32_t>(s->data.size());
        if (end > hi) hi = end;
    }
    if (forceHi > forceLo) { lo = forceLo; hi = forceHi; }
    HexSegment out;
    out.addr = lo & ~0xfu;                       // 16-byte aligned start
    hi = (hi + 15u) & ~0xfu;                     // and a whole number of 16s
    out.data.assign(hi - out.addr, 0xff);
    for (auto* s : segs)
        for (size_t i = 0; i < s->data.size(); ++i)
            out.data[s->addr - out.addr + i] = s->data[i];
    return out;
}

// Who is on the wire right now? The bootloader names itself in the query, and
// on a June 2025 board that name is "MIOS32 Bootloader".
bool inBootloader(Link& lk)
{
    Reply r = lk.exchange(query(lk.deviceId, 0x08), 200);   // app name line 1
    if (!(r.valid && r.cmd == ACK)) return false;
    const std::string s = r.text();
    return s.find("Bootloader") != std::string::npos ||
           s.find("bootloader") != std::string::npos;
}

// Is the application answering yet? Used only to stop waiting early - the
// verification below is what actually decides.
bool inBootloaderReady(Link& lk)
{
    Reply r = lk.exchange(query(lk.deviceId, 0x01), 200);
    return r.valid && r.cmd == ACK && r.text() == "ADIOS";
}

// Walks the operator through the panel shortcut and waits. Deliberately has
// no timeout on the instruction itself: somebody has to walk to the machine.
bool waitForBootloader(Link& lk, Log& log)
{
    if (inBootloader(lk)) { log.info("the board is already in bootloader mode"); return true; }

    log.warn("PLEASE PUT THE MACHINE IN BOOTLOADER MODE");
    log.warn("hold LAST + UP and switch the machine on");

    const int seconds = 120;                 // two minutes of patience
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (inBootloader(lk)) {
            log.status("");
            log.info("bootloader detected");
            // Safe to let go already: from here every SysEx command we send
            // resets the bootloader's own 2 s timer (bsl_sysex.c:189), so it
            // stays put whether the buttons are held or not.
            log.ok("you can release the panel buttons");
            // From here the operator should not walk away: the sequence is
            // about to rewrite the boot region. Nothing is written YET - the
            // board would still restart intact - but locking here rather than
            // two steps later is the version a tester can reason about.
            log.lock(true);
            return true;
        }
        const int left = (int)std::chrono::duration_cast<std::chrono::seconds>(
                             deadline - std::chrono::steady_clock::now()).count();
        log.status("waiting for the machine - %d:%02d left", left / 60, left % 60);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    log.status("");
    log.err("no bootloader appeared - was LAST + UP held while switching on?");
    return false;
}

// Sends one segment in ascending address order. The old bootloader erases each
// page as it reaches it (bsl_sysex.c WriteMem), so ascending order is not an
// optimisation, it is what makes the erase land before the data.
bool sendSegment(Link& lk, const HexSegment& seg, const char* what, Log& log)
{
    const size_t total = seg.data.size();
    for (size_t off = 0; off < total; off += BLOCK) {
        const size_t n = (total - off < BLOCK) ? (total - off) : BLOCK;
        const uint32_t addr = seg.addr + static_cast<uint32_t>(off);

        Reply r = writeBlock(lk, addr, seg.data.data() + off, n, 3000);
        if (!r.valid)          { log.err("%s: no answer at 0x%08X", what, addr); return false; }
        if (r.cmd == DISACK)   { log.err("%s: refused at 0x%08X (0x%02X)", what, addr, r.arg); return false; }

        log.progress(off + n, total);
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------

bool upgrade(Link& lk, const UpgradeImages& img, Log& log)
{
    HexImage migration, application;
    std::string err;
    if (!loadHex(img.migrationHex, migration, err))  { log.err("%s", err.c_str()); return false; }
    if (!loadHex(img.applicationHex, application, err)) { log.err("%s", err.c_str()); return false; }

    // --- split the migration image at the boundary -------------------------
    // below  = the new bootloader, for the TOOL to write (stage 2)
    // at/above = the tool itself, for the OLD bootloader to write (stage 1)
    // BOTH sides can hold several segments - a hex is not one contiguous run,
    // and taking only the last one sent 372 bytes of bootloader instead of 7470.
    std::vector<const HexSegment*> toolSegs, bslSegs;
    for (const auto& s : migration.segments)
        (s.addr >= 0x08000000 + img.legacyBoundary ? toolSegs : bslSegs).push_back(&s);

    if (toolSegs.empty()) { log.err("no tool stage above 0x%08X in %s",
                             0x08000000 + img.legacyBoundary, img.migrationHex.c_str()); return false; }
    if (bslSegs.empty())  { log.err("no bootloader image below the boundary in %s",
                             img.migrationHex.c_str()); return false; }

    auto span = [](const std::vector<const HexSegment*>& v, size_t& bytes) {
        uint32_t lo = 0xFFFFFFFFu, hi = 0; bytes = 0;
        for (auto* s : v) {
            if (s->addr < lo) lo = s->addr;
            const uint32_t end = s->addr + static_cast<uint32_t>(s->data.size());
            if (end > hi) hi = end;
            bytes += s->data.size();
        }
        return std::pair<uint32_t,uint32_t>(lo, hi);
    };
    size_t toolBytes = 0, bslBytes = 0;
    auto toolSpan = span(toolSegs, toolBytes);
    auto bslSpan  = span(bslSegs,  bslBytes);

    const HexSegment* tool = toolSegs.front();   // the tool entry, lowest address

    log.info("tool      0x%08X..0x%08X  %zu bytes in %zu segment(s)",
             toolSpan.first, toolSpan.second - 1, toolBytes, toolSegs.size());
    log.info("new BSL   0x%08X..0x%08X  %zu bytes in %zu segment(s)",
             bslSpan.first, bslSpan.second - 1, bslBytes, bslSegs.size());

    // --- 1. get the board into its bootloader ------------------------------
    // NOT by asking it to restart: the 2 s window that would catch us is
    // skipped entirely when fastboot is set in the info block, and the boards
    // in the field have it. The panel shortcut has none of that fragility -
    // holding the buttons keeps the bootloader in its service loop for as
    // long as the operator wants (main.c: hold_mode_active_after_reset).
    log.step(0, 1);
    if (!waitForBootloader(lk, log)) return false;

    // --- 2. the tool ------------------------------------------------------
    // No window to race now: the buttons hold the bootloader, and from the
    // first written block BSL_SYSEX_HaltStateGet() holds it instead.
    log.info("uploading the tool");
    HexSegment toolFlat = flatten(toolSegs);
    if (!sendSegment(lk, toolFlat, "tool", log)) return false;

    // --- 3. the signpost ---------------------------------------------------
    // The blind jump goes to the boundary, and the tool is not there. Copy its
    // vector table to the boundary so the jump bounces into it. Only needed on
    // a legacy core - a modern one gets SysEx 0x03 instead.
    log.info("writing the entry signpost at 0x%08X", 0x08000000 + img.legacyBoundary);
    {
        const size_t n = (tool->data.size() < 256) ? tool->data.size() : 256;
        Reply r = writeBlock(lk, 0x08000000 + img.legacyBoundary, tool->data.data(), n, 3000);
        if (!r.valid || r.cmd != ACK) { log.err("signpost not acknowledged"); return false; }
    }

    // --- 4. let it jump, and wait for the tool -----------------------------
    log.info("starting the tool");
    lk.send(reset(lk.deviceId));

    // THEN SHUT UP. The old bootloader resets its 2 s timer on EVERY SysEx
    // command it receives - the very first line of BSL_SYSEX_Cmd, before the
    // command is even looked at (bsl_sysex.c:189, "wait 2 additional seconds
    // whenever a SysEx message has been received"). And ReleaseHaltState does
    // not jump: it only clears the halt flag and lets the loop time out.
    // So polling for the tool KEEPS THE BOOTLOADER ALIVE - asking whether it
    // has left is what stops it leaving. Silence is the instruction.
    log.info("    (staying quiet so its timer can expire)");
    std::this_thread::sleep_for(std::chrono::milliseconds(2600));

    // The tool answers query 0x0b with "UPDATER" - it is a current-generation
    // core, so this sub-command exists on it. A legacy core never answers it,
    // which is what makes the check unambiguous: if we see UPDATER, the
    // signpost worked and the blind jump landed on the tool.
    // The tool is an ADIOS core, the old bootloader is a MIOS32 one - so the
    // OS name alone tells them apart, and unlike query 0x0b it exists on both.
    if (!findTool(lk, 12, log)) {
        log.err("nothing ADIOS answered on any device id - the jump did not");
        log.err("land on the tool. Signpost wrong, or the tool does not run.");
        return false;
    }
    if (!waitForCore(lk, "UPDATER", 5, log)) {
        log.err("an ADIOS core answered but does not call itself UPDATER");
        return false;
    }
    log.info("tool is running");
    log.step(0, 2);
    log.step(1, 1);

    // --- 5. stage 2: the tool writes the new bootloader --------------------
    log.info("writing the new bootloader");
    {
        // the WHOLE region, 0x08000000 to the new boundary: every page gets
        // erased and rewritten, so nothing of a previous partial attempt is
        // left underneath. The updater refuses anything above boundary-1, so
        // this is exactly its window and not a byte more.
        HexSegment flat = flatten(bslSegs, 0x08000000u,
                                  0x08000000u + img.newBoundary);
        log.info("    clearing and writing 0x08000000..0x%08X",
                 0x08000000u + img.newBoundary - 1);
        if (!sendSegment(lk, flat, "bootloader", log)) return false;
    }

    // --- 6. finalize: info block restored, high page translated ------------
    // The tool does NOT reset here - it stays resident so the state is visible
    // from outside (BSL_SYSEX_ReleaseHaltState, case A). Our high-page fixup
    // runs in that same step.
    log.info("finalizing - info block restored, high page translated");
    lk.send(reset(lk.deviceId));

    // Case A does NOT reset: the tool stays resident so the state is readable
    // from outside. So it must still answer UPDATER when it is done.
    if (!waitForCore(lk, "UPDATER", 10, log)) return false;

    // --- 7. the application, relayed through the tool ----------------------
    // This second 0x7f falls into case B: the tool sets the reboot flag and
    // resets, so the FRESH bootloader receives the application.
    log.step(1, 2);
    log.step(2, 1);
    log.info("handing over to the new bootloader");
    lk.send(reset(lk.deviceId));

    // Case B: the tool sets the reboot flag and resets, so the FRESH bootloader
    // comes up and receives the application. It announces itself as "BSL".
    if (!waitForCore(lk, "BSL", 10, log)) {
        log.err("the new bootloader did not come up");
        return false;
    }
    log.info("new bootloader is running");

    log.info("uploading the application");
    for (const auto& s : application.segments)
        if (!sendSegment(lk, s, "application", log)) return false;

    log.info("starting the application");
    lk.send(reset(lk.deviceId));

    // --- 8. check the result rather than asking the operator to -------------
    // The board restarts by itself, so there is nothing to switch off and on.
    // Measured: a restart takes ~3.3 s end to end, most of it the application
    // bringing its display up.
    log.status("waiting for the board to come back");
    for (int i = 0; i < 8 && !inBootloaderReady(lk); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
    log.status("");

    Reply os = lk.exchange(query(lk.deviceId, 0x01), 400);
    Reply bd = lk.exchange(query(lk.deviceId, 0x0a), 400);   // boundary: new cores only
    Reply pg = lk.exchange(ping(Target::App, lk.deviceId), 400);

    const bool adios    = os.valid && os.cmd == ACK && os.text() == "ADIOS";
    const bool boundary = bd.valid && bd.cmd == ACK && !bd.text().empty();
    const bool unit     = pg.valid && pg.cmd == ACK && (pg.arg == 0x50 || pg.arg == 0x62);

    log.step(2, 2);
    log.lock(false);

    if (adios)    log.info("  OS         %s", os.text().c_str());
    if (boundary) log.info("  boundary   0x%s", bd.text().c_str());
    if (unit)     log.info("  machine    %s", pg.arg == 0x62 ? "TR-626" : "TR-505");

    if (adios && boundary && unit) {
        // The application answering its own ping is the real proof: it only
        // gets that far if the magic in the high page named a machine it
        // recognises. A board whose page was left wrong would be sitting on
        // the unit-select question instead, answering nothing.
        log.ok("UPDATE DONE WITH SUCCESS");
        return true;
    }

    log.err("the board came back but does not check out - do NOT let it format");
    return false;
}

} // namespace tr5x6
