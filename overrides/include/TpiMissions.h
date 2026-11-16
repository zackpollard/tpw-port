// TpiMissions.h: the TPI Worlds and Rides mod's obstacle missions (TPW_MOD_HOOK tpi_worlds). In Theme Park Inc an
// obstacle (Land of Invention's gate, Polar Zone's swamp and tunnel, Arabian Nights' rock fall, cliff and tunnel) stayed
// until its mission was done: golden tickets, research and staff with the right abilities (its Data/missions/missNN.sam),
// or for the gate a number of visitors in the park. Once done, the obstacle cleared and the park's hoarding took in the
// land the mission's BoundaryClicks drew. The converter (assets/tpi_missions.h) writes the missions of each of the mod's
// worlds whose obstacle it named in the terrain (a "tpishut" or "tpigate" footprint) into TpiMissions.sam, in the mod's
// own line format; CWorld::SetupNewLevel reads it (TpiParseMissions) into gTpiMissions.
// Theme Park World has no staff abilities and no obstacle research, so a mission's needs are mapped (TpiMissionNeedsOf):
// each Theme Park Inc staff type to the nearest Theme Park World staff kind, an ability to one of that kind at pay
// grade 3 or more ("skilled": hires come at grade 1 to 3 in these worlds, training takes them to 4), research to one
// researcher on the staff, golden tickets to the tickets won in this park (not spent).
// The missions done are one word in the park file: the litter mark time of map cell 0x3fff (127, 127), a corner no park
// reaches and no handyman marks, saved and loaded with every cell; kTpiMissionTag in its top byte, bit n for mission n.
// A word without the tag (a park saved before 0.5.6) takes, once, each mission whose obstacle the park's land already
// cleared under 0.5.5's rule (TpiMigrateMissions), so nothing cleared then stands again.
// Header only and pure, so the host test sees what the game does.
#pragma once


// TPW_PORT_FIX mods TpiMissions | Mac: no such header; Theme Park World has no obstacles or missions | Port: the obstacle missions' data, needs, saved word, migration and the hoarding extension planner the mod's worlds use (TPW_MOD_HOOK tpi_worlds) | Why: tpw-mods tpi-worlds; Theme Park Inc cleared its obstacles with missions (the maintainer, 0.5.6)

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "TpiObstacleGate.h"

namespace tpw {

// The file the converter writes into each of the mod's worlds (data/levels/<world>/TpiMissions.sam).
inline constexpr char kTpiMissionsFile[] = "TpiMissions.sam";
inline constexpr int32_t kTpiMaxMissions = 8;        // a world's (Theme Park Inc's have 3 at most with an obstacle)
inline constexpr int32_t kTpiMaxMissionStaff = 8;    // a mission's staff lines (Theme Park Inc's have 5 at most)
inline constexpr int32_t kTpiMaxMissionNumber = 23;  // bits 0 to 23 of the word
inline constexpr uint32_t kTpiMissionTag = 0xA5000000u;
inline constexpr uint32_t kTpiMissionTagMask = 0xFF000000u;
inline constexpr int32_t kTpiSkilledGrade = 3;       // CStaff::mCurrentPayGrade, 0 to 4
inline constexpr uint32_t kTpiMissionCell = 0x3fff;  // the map cell whose litter mark time holds the word

// Theme Park World's staff kinds (CThing::mKind).
enum : int32_t { kTpiKindMechanic = 4, kTpiKindHandyman = 5, kTpiKindEntertainer = 6, kTpiKindGuard = 7, kTpiKindResearcher = 8 };

struct TpiMissionStaff {
    int32_t tpiType = 0;   // ObstacleStaff[i].StaffType
    int32_t ability = 0;   // ObstacleStaff[i].Ability (kept for the record; Theme Park World has no abilities)
    int32_t count = 0;     // ObstacleStaff[i].Number
};

struct TpiMission {
    int32_t number = 0;                  // Info.Number, 0 to kTpiMaxMissionNumber
    int32_t obstacle = 0;                // Obstacle.Type (the obstacle's Info.Id)
    int32_t x = 0, y = 0, w = 0, h = 0;  // the obstacle's footprint as the terrain names it
    int32_t tickets = 0;                 // Obstacle.GoldenTickets
    int32_t visitors = 0;                // ExitStatistic Type 8 TargetValue (visitors in the park)
    int32_t research = 0;                // Obstacle.ResearchEffort
    int32_t staffCount = 0;
    TpiMissionStaff staff[kTpiMaxMissionStaff];
    int32_t fenceCount = 0;              // BoundaryClicks, from (right, bottom) as Hoardings.sam's
    int32_t fx[64] = {}, fy[64] = {};
};

struct TpiMissionTable {
    int32_t count = 0;   // in mission number order
    TpiMission missions[kTpiMaxMissions];
};

// The obstacle missions of the world being played (TPW_MOD_HOOK tpi_worlds, gModTpiWorlds): CWorld::SetupNewLevel reads
// them from the world's TpiMissions.sam in one of the mod's worlds (TpiParseMissions); empty in any other.
inline TpiMissionTable gTpiMissions;

namespace detail {
inline bool MissionInts(const char*& at, const char* end, int32_t* out, int32_t n) {
    for (int32_t i = 0; i < n; ++i) {
        while (at < end && (*at == ' ' || *at == '\t')) ++at;
        if (at >= end || *at == '\n' || *at == '\r') return false;
        char buf[16];
        int32_t len = 0;
        while (at < end && len < 15 && *at != ' ' && *at != '\t' && *at != '\n' && *at != '\r') buf[len++] = *at++;
        buf[len] = 0;
        char* stop = nullptr;
        const long v = std::strtol(buf, &stop, 10);
        if (stop == buf || *stop != 0 || v < -100000 || v > 100000) return false;
        out[i] = static_cast<int32_t>(v);
    }
    return true;
}
inline TpiMission* MissionByNumber(TpiMissionTable& table, int32_t number) {
    for (int32_t i = 0; i < table.count; ++i)
        if (table.missions[i].number == number) return &table.missions[i];
    return nullptr;
}
}  // namespace detail

// Reads TpiMissions.sam's lines into `table` (emptied first):
//   Mission N OBSTACLE X Y W H TICKETS VISITORS RESEARCH
//   Staff N TPITYPE ABILITY COUNT
//   Fence N COUNT dx dy ... (COUNT pairs)
// '#' starts a comment; a line that does not read, a footprint outside the map, a Staff or Fence line of a mission not
// given before, or a mission past kTpiMaxMissions is skipped. Missions are kept in number order. True when one at least.
inline bool TpiParseMissions(const char* text, size_t size, TpiMissionTable& table) {
    table = TpiMissionTable{};
    const char* at = text;
    const char* end = text + size;
    while (text != nullptr && at < end) {
        const char* line = at;
        while (at < end && *at != '\n') ++at;
        const char* lineEnd = at;
        if (at < end) ++at;
        for (const char* c = line; c < lineEnd; ++c)
            if (*c == '#') { lineEnd = c; break; }
        const char* p = line;
        while (p < lineEnd && (*p == ' ' || *p == '\t')) ++p;
        auto word = [&](const char* w) {
            const size_t n = std::strlen(w);
            if (static_cast<size_t>(lineEnd - p) > n && std::strncmp(p, w, n) == 0 && (p[n] == ' ' || p[n] == '\t')) {
                p += n;
                return true;
            }
            return false;
        };
        int32_t v[9];
        if (word("Mission")) {
            if (!detail::MissionInts(p, lineEnd, v, 9) || table.count >= kTpiMaxMissions) continue;
            if (v[0] < 0 || v[0] > kTpiMaxMissionNumber || detail::MissionByNumber(table, v[0]) != nullptr) continue;
            if (v[2] < 0 || v[3] < 0 || v[4] <= 0 || v[5] <= 0 || v[2] + v[4] > 128 || v[3] + v[5] > 128) continue;
            int32_t slot = table.count;
            while (slot > 0 && table.missions[slot - 1].number > v[0]) {
                table.missions[slot] = table.missions[slot - 1];
                --slot;
            }
            TpiMission m;
            m.number = v[0];
            m.obstacle = v[1];
            m.x = v[2];
            m.y = v[3];
            m.w = v[4];
            m.h = v[5];
            m.tickets = v[6] < 0 ? 0 : v[6];
            m.visitors = v[7] < 0 ? 0 : v[7];
            m.research = v[8] < 0 ? 0 : v[8];
            table.missions[slot] = m;
            ++table.count;
        } else if (word("Staff")) {
            if (!detail::MissionInts(p, lineEnd, v, 4)) continue;
            TpiMission* m = detail::MissionByNumber(table, v[0]);
            if (m == nullptr || m->staffCount >= kTpiMaxMissionStaff || v[3] <= 0) continue;
            m->staff[m->staffCount++] = TpiMissionStaff{v[1], v[2], v[3]};
        } else if (word("Fence")) {
            if (!detail::MissionInts(p, lineEnd, v, 2)) continue;
            TpiMission* m = detail::MissionByNumber(table, v[0]);
            if (m == nullptr || v[1] < 3 || v[1] > 64) continue;
            int32_t xy[128];
            if (!detail::MissionInts(p, lineEnd, xy, 2 * v[1])) continue;
            m->fenceCount = v[1];
            for (int32_t i = 0; i < v[1]; ++i) {
                m->fx[i] = xy[2 * i];
                m->fy[i] = xy[2 * i + 1];
            }
        }
    }
    return table.count > 0;
}

// The Theme Park World staff kind standing in for a Theme Park Inc staff type: 0 (cleaner) and 5 (gardener) handyman,
// who sweeps and tends the gardens; 1 (mechanic, engineer) mechanic; 2 entertainer; 3 guard; 4 researcher; 0 for none.
inline int32_t TpiStaffKindFor(int32_t tpiType) {
    switch (tpiType) {
    case 0: case 5: return kTpiKindHandyman;
    case 1: return kTpiKindMechanic;
    case 2: return kTpiKindEntertainer;
    case 3: return kTpiKindGuard;
    case 4: return kTpiKindResearcher;
    default: return 0;
    }
}

// What a mission needs, in Theme Park World's terms, and what the park has; staff indexed by kind (4 to 8).
struct TpiMissionNeeds {
    int32_t tickets = 0, visitors = 0;
    int32_t skilled[9] = {};   // skilled staff of each kind (researchers: any researcher)
};
struct TpiParkNow {
    int32_t tickets = 0, visitors = 0;
    int32_t skilled[9] = {};   // live staff of each kind at kTpiSkilledGrade or more
    int32_t staff[9] = {};     // live staff of each kind
};

inline TpiMissionNeeds TpiMissionNeedsOf(const TpiMission& m) {
    TpiMissionNeeds needs;
    needs.tickets = m.tickets;
    needs.visitors = m.visitors;
    for (int32_t i = 0; i < m.staffCount; ++i) {
        const int32_t kind = TpiStaffKindFor(m.staff[i].tpiType);
        if (kind != 0) needs.skilled[kind] += m.staff[i].count;
    }
    if (m.research > 0 && needs.skilled[kTpiKindResearcher] < 1) needs.skilled[kTpiKindResearcher] = 1;
    return needs;
}

// Whether the park has what the mission needs. Researchers count at any grade, the other kinds when skilled.
inline bool TpiMissionMet(const TpiMissionNeeds& needs, const TpiParkNow& now) {
    if (now.tickets < needs.tickets || now.visitors < needs.visitors) return false;
    for (int32_t kind = kTpiKindMechanic; kind <= kTpiKindGuard; ++kind)
        if (now.skilled[kind] < needs.skilled[kind]) return false;
    return now.staff[kTpiKindResearcher] >= needs.skilled[kTpiKindResearcher];
}

// The saved word.
inline bool TpiMissionsTagged(uint32_t word) { return (word & kTpiMissionTagMask) == kTpiMissionTag; }
inline bool TpiMissionDone(uint32_t word, int32_t number) {
    return TpiMissionsTagged(word) && number >= 0 && number <= kTpiMaxMissionNumber && ((word >> number) & 1u) != 0;
}
inline uint32_t TpiMissionSetDone(uint32_t word, int32_t number) {
    const uint32_t bits = TpiMissionsTagged(word) ? (word & ~kTpiMissionTagMask) : 0u;
    if (number < 0 || number > kTpiMaxMissionNumber) return kTpiMissionTag | bits;
    return kTpiMissionTag | bits | (1u << number);
}

// The mission whose obstacle's footprint holds a cell of (x, y, w, h) (a node of the obstacle: a gate leaf, a part of
// the rubble): its index, or -1.
inline int32_t TpiMissionAt(const TpiMissionTable& table, int32_t x, int32_t y, int32_t w, int32_t h) {
    for (int32_t i = 0; i < table.count; ++i) {
        const TpiMission& m = table.missions[i];
        if (x < m.x + m.w && m.x < x + w && y < m.y + m.h && m.y < y + h) return i;
    }
    return -1;
}

// Whether an obstacle node of footprint (x, y, w, h) is gone: its mission done when it has one and the word is tagged; else the land
// rule (`landRule()`, 0.5.5's: TpiGateOpen over the footprint).
template <class LandRule>
inline bool TpiObstacleGone(const TpiMissionTable& table, uint32_t word, int32_t x, int32_t y, int32_t w, int32_t h,
                            LandRule landRule) {
    const int32_t i = TpiMissionAt(table, x, y, w, h);
    if (i < 0 || !TpiMissionsTagged(word)) return landRule();
    return TpiMissionDone(word, table.missions[i].number);
}

// The word for a park whose word is untagged: each mission whose obstacle the land rule says is gone (`gone(mission)`)
// is done. An already tagged word is kept.
template <class Gone>
inline uint32_t TpiMigrateMissions(const TpiMissionTable& table, uint32_t word, Gone gone) {
    if (TpiMissionsTagged(word)) return word;
    uint32_t out = kTpiMissionTag;
    for (int32_t i = 0; i < table.count; ++i)
        if (gone(table.missions[i])) out = TpiMissionSetDone(out, table.missions[i].number);
    return out;
}

// The mission being played: the first one not done, in number order; -1 when all are.
inline int32_t TpiActiveMission(const TpiMissionTable& table, uint32_t word) {
    for (int32_t i = 0; i < table.count; ++i)
        if (!TpiMissionDone(word, table.missions[i].number)) return i;
    return -1;
}

// The hoarding a mission draws, as a TpiStartFence (its clicks from the level's right and bottom): the land inside it is
// TpiInsideFence's.
inline TpiStartFence TpiMissionFence(const TpiMission& m, int32_t left, int32_t right, int32_t bottom) {
    TpiStartFence fence;
    fence.left = left;
    fence.right = right;
    fence.bottom = bottom;
    for (int32_t i = 0; i < m.fenceCount; ++i) TpiFenceClick(fence, right + m.fx[i], bottom + m.fy[i]);
    return fence;
}

// A rectangle of hoarding for LLCC_IsHoardingEditLegal and LLCC_ReviseHoarding (corners lox, loy and hix, hiy): it makes
// the cells x lox + 1 to hix and y loy + 1 to hiy the park's (the fence through the corners' far edges, TpiInsideFence's
// rule).
struct TpiGrantRect {
    int32_t lox = 0, loy = 0, hix = 0, hiy = 0;
    int32_t gain = 0;   // cells it adds that the mission's fence holds
};

// The rectangles that extend the park towards the mission's land, best first, into `out` (at most `maxOut`); the count.
// `want[x + 128 * y]` is 1 for a cell inside the mission's fence, `owned[...]` 1 for a cell the park has (flag 0x40 clear
// or hoarding). A rectangle's corners lie on the hoarding's lattice (x and y of the parity of `px`, `py`: the level's
// right and bottom, as Buy Land snaps them), its sides 4 cells at least, x from 1 to `maxX` and y from 1 to `maxY`. Its
// cells must all be wanted or owned, one at least not owned and wanted, and its edge (the closed rectangle) must touch
// owned land, so that each is a purchase Buy Land could make towards the mission's land and no further. Best is the
// most cells gained, then the smaller rectangle.
inline int32_t TpiPlanGrants(const uint8_t* want, const uint8_t* owned, int32_t px, int32_t py, int32_t maxX, int32_t maxY,
                             TpiGrantRect* out, int32_t maxOut) {
    if (maxOut <= 0) return 0;
    // Prefix sums over (x, y) of: a cell neither wanted nor owned (bad), wanted and not owned (gain), owned.
    static int32_t bad[129][129], gain[129][129], own[129][129];
    int32_t gx0 = 128, gy0 = 128, gx1 = -1, gy1 = -1;
    for (int32_t y = 0; y <= 128; ++y) bad[y][0] = gain[y][0] = own[y][0] = 0;
    for (int32_t x = 0; x <= 128; ++x) bad[0][x] = gain[0][x] = own[0][x] = 0;
    for (int32_t y = 0; y < 128; ++y)
        for (int32_t x = 0; x < 128; ++x) {
            const bool o = owned[x + 128 * y] != 0, w = want[x + 128 * y] != 0;
            const int32_t b = (!o && !w) ? 1 : 0, g = (w && !o) ? 1 : 0;
            if (g) {
                if (x < gx0) gx0 = x;
                if (y < gy0) gy0 = y;
                if (x > gx1) gx1 = x;
                if (y > gy1) gy1 = y;
            }
            bad[y + 1][x + 1] = bad[y][x + 1] + bad[y + 1][x] - bad[y][x] + b;
            gain[y + 1][x + 1] = gain[y][x + 1] + gain[y + 1][x] - gain[y][x] + g;
            own[y + 1][x + 1] = own[y][x + 1] + own[y + 1][x] - own[y][x] + (o ? 1 : 0);
        }
    if (gx1 < 0) return 0;
    auto sum = [](const int32_t (&s)[129][129], int32_t x0, int32_t y0, int32_t x1, int32_t y1) {   // cells x0..x1, y0..y1
        return s[y1 + 1][x1 + 1] - s[y0][x1 + 1] - s[y1 + 1][x0] + s[y0][x0];
    };
    auto first = [](int32_t from, int32_t parity) { return (from & 1) == (parity & 1) ? from : from + 1; };
    int32_t n = 0;
    // A rectangle gaining a cell (gx, gy) has lox < gx <= hix: so lox from gx0 - 1 - (room) down; bounded by the map.
    const int32_t lx0 = first(gx0 - 64 < 1 ? 1 : gx0 - 64, px), ly0 = first(gy0 - 64 < 1 ? 1 : gy0 - 64, py);
    for (int32_t lox = lx0; lox <= gx1 - 1 && lox <= maxX; lox += 2)
        for (int32_t hix = first(lox + 4 > gx0 ? lox + 4 : gx0, px); hix <= maxX && hix <= 127; hix += 2) {
            for (int32_t loy = ly0; loy <= gy1 - 1 && loy <= maxY; loy += 2)
                for (int32_t hiy = first(loy + 4 > gy0 ? loy + 4 : gy0, py); hiy <= maxY && hiy <= 127; hiy += 2) {
                    if (sum(bad, lox + 1, loy + 1, hix, hiy) != 0) break;   // taller only adds more cells
                    const int32_t g = sum(gain, lox + 1, loy + 1, hix, hiy);
                    if (g == 0 || sum(own, lox, loy, hix, hiy) == 0) continue;
                    TpiGrantRect r{lox, loy, hix, hiy, g};
                    const int32_t area = (hix - lox) * (hiy - loy);
                    int32_t at = n < maxOut ? n : maxOut - 1;
                    if (n >= maxOut) {
                        const TpiGrantRect& last = out[maxOut - 1];
                        if (g < last.gain || (g == last.gain && area >= (last.hix - last.lox) * (last.hiy - last.loy))) continue;
                    } else {
                        ++n;
                    }
                    while (at > 0 && (out[at - 1].gain < g ||
                                      (out[at - 1].gain == g &&
                                       (out[at - 1].hix - out[at - 1].lox) * (out[at - 1].hiy - out[at - 1].loy) > area))) {
                        out[at] = out[at - 1];
                        --at;
                    }
                    out[at] = r;
                }
        }
    return n;
}

// The player's text for a mission (ASCII, the mod's own words): what clears the obstacle and how far the park is.
inline const char* TpiObstacleName(int32_t obstacle) {
    switch (obstacle) {
    case 1700: return "the gate";
    case 2700: return "the swamp";
    case 2703: case 3703: return "the tunnel";
    case 3700: return "the rock fall";
    case 3702: return "the cliff";
    default: return "the obstacle";
    }
}

namespace detail {
inline void MissionAppend(char* out, size_t size, size_t& len, const char* text) {
    while (*text != 0 && len + 1 < size) out[len++] = *text++;
    out[len] = 0;
}
inline void MissionNeed(char* out, size_t size, size_t& len, bool& firstNeed, int32_t need, int32_t have,
                        const char* one, const char* many) {
    if (need <= 0) return;
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s%d %s (%d)", firstNeed ? "" : ", ", static_cast<int>(need), need == 1 ? one : many,
                  static_cast<int>(have));
    firstNeed = false;
    MissionAppend(out, size, len, buf);
}
}  // namespace detail

// "To clear the rock fall: 1 golden ticket (0), 4 skilled mechanics (1), ..." with what the park has in brackets.
inline void TpiMissionText(const TpiMission& m, const TpiParkNow& now, char* out, size_t size) {
    if (size == 0) return;
    size_t len = 0;
    out[0] = 0;
    const TpiMissionNeeds needs = TpiMissionNeedsOf(m);
    detail::MissionAppend(out, size, len, "To clear ");
    detail::MissionAppend(out, size, len, TpiObstacleName(m.obstacle));
    detail::MissionAppend(out, size, len, ", the park needs: ");
    bool firstNeed = true;
    detail::MissionNeed(out, size, len, firstNeed, needs.visitors, now.visitors, "visitor in the park", "visitors in the park");
    detail::MissionNeed(out, size, len, firstNeed, needs.tickets, now.tickets, "golden ticket", "golden tickets");
    detail::MissionNeed(out, size, len, firstNeed, needs.skilled[kTpiKindMechanic], now.skilled[kTpiKindMechanic],
                        "skilled mechanic", "skilled mechanics");
    detail::MissionNeed(out, size, len, firstNeed, needs.skilled[kTpiKindHandyman], now.skilled[kTpiKindHandyman],
                        "skilled handyman", "skilled handymen");
    detail::MissionNeed(out, size, len, firstNeed, needs.skilled[kTpiKindEntertainer], now.skilled[kTpiKindEntertainer],
                        "skilled entertainer", "skilled entertainers");
    detail::MissionNeed(out, size, len, firstNeed, needs.skilled[kTpiKindGuard], now.skilled[kTpiKindGuard],
                        "skilled guard", "skilled guards");
    detail::MissionNeed(out, size, len, firstNeed, needs.skilled[kTpiKindResearcher], now.staff[kTpiKindResearcher],
                        "researcher", "researchers");
    detail::MissionAppend(out, size, len, ". You have the numbers in brackets.");
    if (needs.skilled[kTpiKindMechanic] + needs.skilled[kTpiKindHandyman] + needs.skilled[kTpiKindEntertainer] +
            needs.skilled[kTpiKindGuard] > 0)
        detail::MissionAppend(out, size, len, " Skilled staff have a skill bar three fifths full or more.");
}

// "The rock fall is cleared and the park's land grows." for a mission just done.
inline void TpiMissionDoneText(const TpiMission& m, char* out, size_t size) {
    if (size == 0) return;
    const char* name = TpiObstacleName(m.obstacle);
    char buf[160];
    std::snprintf(buf, sizeof buf, "Well done! %c%s is cleared%s.", name[0] - 32, name + 1,
                  m.fenceCount >= 3 ? " and the park's land grows" : "");
    size_t len = 0;
    out[0] = 0;
    detail::MissionAppend(out, size, len, buf);
}

}  // namespace tpw

// The game's side (src/game/TPEngine.cpp): the load step (CSaveLoad::PostLoadOperations) and the turn's check
// (CWorld::DoGameTurn, every 30th turn).
void TpiMigrateParkMissions();
void TpiMissionsTurn();

