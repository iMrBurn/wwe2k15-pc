# WWE 2K15 Universe mode: static study (code, data, what can be improved)

Status 2026-11-09, from a memory dump of the title screen (strings with addresses), the generated TU4 code and `wuniData.pac`. Nothing was changed in the game, the port or any save.
Addresses are TU4 (`sub_8xxxxxxx`). Same Yuke's engine as WWE '12 (`C:\Dev\wwe12-pc\docs\univers.md`): the module was moved from 0x826BA000 (WWE '12) to **0x82A5xxxx-0x82A9Cxxx** in 2K15.

## 1. Where the code is (found by the debug strings the developers left in the exe)
| What | Function (TU4) | Evidence |
|---|---|---|
| Daily advance: "Start of day" / "End of day", "Daily random value", "can't overwrite no.1 contender" | `sub_82A63708` (1156 lines) | strings 0x8205BE78 / BF30 / BF44 / BE94 |
| Remove disabled CAWs from matches ("Include disable CAS", "Match No = %d.Delete") | `sub_82A6AC50` | 0x8205BFA0 |
| Simulate a show ("Simulate() start/finish") | `sub_82A6EC80`, `sub_82A6EDA8`, `sub_82A6F050` | 0x8205C014 / BFF4 |
| Next-match sequence (`CWWEUniverseNextMatchSequence`) | `sub_82A71850` | 0x8205C1C8 |
| Royal Rumble result ("SetRoyalRumbleResult() Failed") | `sub_82A60958` | 0x8205BED8 |
| No.1 contender handling | `sub_82A5EED0`, `sub_82A64C58` (also in 82A63708) | 0x8205BE94 |
| Story cancel / rival feud delete / pattern | `sub_82A81C28` ("Story Cancel"), `sub_82A820E0` ("Pattern max") | 0x8205C38C, C3AC |
| Story script jump ("JumpActWarp") | `sub_82A8E6E8` | 0x8205D228 |
| Ranking points ("CalcGetPoints() start/end") | `sub_82A920A0` | 0x8205D2B8 |
| Save init: `CWWEUniverseStationed::BatPacReaderCallBack(_SaveInit(_Status))`, achievements (Road_to_WrestleMania, Deadman defeated...) | `sub_82A53978`, `sub_82A57158` | 0x8205BB20.. |
| Data readers: `WWEU/STRE` (StatusRecords) | `sub_82A97BC8` | 0x8205D600 |
| `WWEU/UWBL` (UWBList) | `sub_82A98538` | 0x8205D828 |
| `WWEU/MTDT` (MatchTemplate) | `sub_82A98920` | 0x8205D954 |
| `WWEU/DC20`, `DC30`, `DC60`, `WWEU/MNTB` (menu table) | `sub_82A8C480` (DC20) | 0x8205CE4C.. |
| Story script commands: SetRoyalRumbleWinner, SetRosterID, SetChampion, SetStoryCast, SetFreeRoster, SetPlayerSeries, SetDraft, SetDraft_Near, SetContender_BrandAndBelt (+_Near), SetMostContender_Near, SetWorstContender_Near, SetInjured, SetWinner, SetWinnerFriends, SetFriendEnemy, SetupRosterFSNS | `sub_82A9B2C0` (RR winner), `82A9B388` (RosterID), `82A9B418` (Champion), `82A9B4F8` (StoryCast), `82A9BDC8` (FreeRoster), `82A9BEA0` (PlayerSeries), `82A9C058` (Draft), `82A9C5D0` (Contender_BrandAndBelt) | strings 0x8205D9D0-DB18 |
Also in the exe: "Stats Match Record" / "Stats Ranking Match" / "Stats Ranking SS" (0x82056D08), `ResetUniverse` (0x82057ABC, a menu event), `FighterHistoryRecordFile`, `FighterRecordFile`, `PlayerStats_v1`, `TeamStats_v1`.
Story creation text: "Success to create replacement match of story. Template(%d) rule(%d) month(%d) week(%d)" (0x8205DB68).

## 2. Data (`pac/wuniData.pac`, 223 KB, EPK8; TU4 also has `update/pac/wuniData_pf.pac`)
Directory at 0x800: `UWBL`, `MTDT`, `STYT`, `STRE`, `MNTB` (start in 2 KB sectors after 0x4000, size in 256-byte units). Content is zlib: blocks of `{u32, u32, zlib stream}` inside PACH tables
(`PACH`, count, then 12-byte `{size, id, offset}`).
- UWBL: one zlib stream of **194 records of 4716 bytes** (Universe wrestler/base list) plus a 52-byte "WUE" header block.
- MTDT: 32 small sub-blocks (match templates; 3 on WWE '12's table layout: 17 tables).
- STYT: one block, **150 196 bytes** decompressed (story templates; 136 on WWE '12).
- STRE: status records (new in 2K15, absent on WWE '12). MNTB: 19 menu-table blocks.
Decoded copies and the memory dump are kept outside Git (`work/univ2k15/`: game data, never committed).

## 3. What is new in 2K15 compared with WWE '12
Status/stat records (STRE, "Stats Match Record", ranking stats), `ShowDefaultParameter` reader, story "replacement match" creation, achievements hooked into the Universe day loop, `UWBL`, `DC20/30/60`.
WWE '12 had to track wins itself; 2K15 already has a records structure (to be decoded).

## 4. Objectives, difficulty (estimates, not yet tested in game)
| Objective | Difficulty | Where / how | Save risk |
|---|---|---|---|
| (e) Readable tables (shows, titles, feuds, calendar) | easy-medium | container solved; field layout of MTDT/STYT/UWBL/STRE to decode with the readers' code (`sub_82A98538`, `82A98920`, `82A97BC8`) | none (read only) |
| (c) Restrict wrestlers to some titles | medium | hook the contender choosers (`sub_82A9C5D0` Contender_BrandAndBelt, `82A5EED0`, `82A64C58`) and `SetChampion` (`82A9B418`) with our own table (cvar, default off) | low (changes who is picked, not the save format) |
| (a) Remove the minimum of 15 per show (100 % CAW universe) | medium (not located yet) | the check is not found yet: candidates are the roster handling in `sub_82A63708` / `82A6AC50` ("Include disable CAS", SetFreeRoster `82A9BDC8`) or the UI flow that edits rosters; needs a second memory dump on the roster screen to find the error-text reference | medium: a roster below the minimum may break match generation (empty cards); test on a COPY |
| (b) One major show only | medium-hard | show list/`ShowDefaultParameter`; the generator may assume two brands | medium |
| (d) Spontaneous title defences | medium-hard | story templates (STYT) are data driven with the commands above: adding a template may be possible without code once the format is decoded; otherwise a hook in the card generator | medium |
| Stat tracking (WWE '13 style) | medium (2K15 has records) | read STRE/records; add our own screen only if the game's own one is missing | low |
| Anything with new cutscenes (Create a Story, WWE '13 scenes) | hard | scene data in `evt/*.pac` + chooser code | high |
## 5. Next steps proposed
1. Decode STYT / MTDT / UWBL / STRE fields (python, `work/univ2k15/`).
2. A second memory dump taken inside the Universe roster/edit screens (needs a key script; the user's own save is never touched, use a copy) to find the text/xref of the minimum-roster message.
3. Read `sub_82A63708` (day loop) and the card generator, then prototype (a) behind a cvar, default off.

## 6. File format of `wuniData.pac` (decoded 2026-11-09; formats only, no game content)
- EPK8 header at 0; group directory at 0x800: 12-byte entries `{name[4], start, size}`; first entry = group name `WWEU` (+ flags); the rest are sections `UWBL` (start 0, size 0x46), `MTDT` (9, 0x20),
  `STYT` (0xD, 0x25D), `STRE` (0x59, 1), `MNTB` (0x5A, 0x4C). `start` is in 2 KB sectors after 0x4000, `size` in 256-byte units (the section ends where the next one starts).
- Each section is a PACH: `"PACH"`, count N, 8 bytes of zero, then N 12-byte entries `{size, id, end}`, then the data (base = 16 + 12*N). Block i starts at `end[i-1]` (0 for the first) and is `size[i]` bytes;
  `end` is the next block's start (4-byte aligned). A last entry with `id = 'ZLIB'` (0x42494C5A) describes the section's compression trailer (its `end` is always 4656).
- A compressed block is `{u32 compressed_size, u32 uncompressed_size}` followed by a zlib stream (78 DA ...). Decoded data is little-endian (the game byte-swaps it when it reads it).
Decoded block sizes:
| Section | Blocks | Uncompressed size(s) | Notes |
|---|---|---|---|
| UWBL | 1 data block + 'ZLIB' block | **917 240 bytes** = a PACH of **194 records of 4716 bytes** (id 1..193 + tag) ; 52-byte header block starting "WUE" | records hold small ints and sentinels (0xFF); no text |
| MTDT | 31 blocks + the last ('ZLIB' tag) | 2540 bytes for most (ids 1-12, 17-31), 16 888 / 10 136 / 4 228 / 10 136 (ids 13-16) | starts with magic `"WMT "` and a count; records of variable size; empty participant slots are the word `0xFFFF00FF` |
| STYT | 1 block | **7 928 232 bytes** | story templates (the game prints `StoryNo,Trigger,CastA..CastF`) |
| STRE | 1 block | 212 bytes | status records header/table |
| MNTB | 18 blocks + 'ZLIB' | 540, 140, 1216, 34 888, 29 764, 3 928 (x3), 348, 5 540, 2 884, 13 724, 124, 44, 17 268, 1 108, 3 276, 3 096, 220 | menu tables (ids 1, 4, 6-21) |
Consumers (to read next for the field meaning): data readers `sub_82A97D20` (STRE), `82A97E38` (ShowDefaultParameter), `82A98410` (STYT), `82A987C0` (UWBL), `82A988C0` (MTDT), `82A8D1E8` (menu data table);
the readers are thin virtual wrappers, the fields are used by the day loop `sub_82A63708` (about 100 calls into `82A58xxx-82A74xxx`: roster/card/story helpers such as `82A5B618`, `82A58630`, `82A62B10`, `82A65FF8`).
Full field-by-field decoding is not done yet.

## 7. Further format notes (2026-11-09)
- STRE (`WWEU/STRE`, reader `sub_82A97D20`): payload = 8-byte block header + **204 bytes** = **17 rows x 6 u16** (the byte-swap routine `sub_82A97C08` swaps 17 x 6 halfwords) copied into the Univers data object (+56).
- MTDT (reader `sub_82A988C0`): **32 sub-tables** (PACH entries 0..31) loaded into a 32-pointer array; each decoded block starts with the magic `"WMT "`, a count and variable-size records whose empty
  participant slots are `0xFFFF00FF`; tables 12-15 are larger (16 888 / 10 136 / 4 228 / 10 136 bytes).
- ShowDefaultParameter (`CWWEUniverseDataReader_ShowDefaultParameter`, reader `sub_82A97E38`): reads a PACH of 4 blocks, takes the blocks 2 and 3 as two tables stored at this+56 / this+60 (converted by `sub_82A7F6C8`,
  merged by `sub_82A74E90`). It is not in `wuniData.pac`: the Universe starting data is `pac/misc05_save.pac` (group `BATL`, section `SAVE`; group entries there are 16 bytes `{name[8], start, size}`, section start 0,
  size 0x1EA x 256 bytes): a PACH of **5 uncompressed blocks**: id 3 (11 216 bytes), id 4 (14 008), id 5 (37 624, begins with an offset table: 0, 0x1310, 0x17A8, 0x1914, 0x1EB8, 0x1EC0, 0x1EFC...),
  id 6 (62 196, begins with the u16 2004 = the starting year, then 20100 / 20300), and a 200-byte `LBTS` block. These are the "new Universe" default values (rosters, shows, dates, relations): the best place to look
  for show/roster parameters (a minimum roster per show may be a value here rather than code). Field meaning not decoded yet.
- Other Univers-related pacs seen: `pac/evt/Universe00-09.pac` (Univers scenes), `evt/Season*.pac`, `evt/Winning*.pac`, `evtdsc/`.

## 8. In-game observations (2026-11-09, separate profile `--user_data_root`, user's save untouched: sha256 of both save trees identical before/after)
- Main menu: PLAY (Exhibition), 2K SHOWCASE, Who Got NXT, UNIVERSE, CREATIONS... A fresh profile asks to create save data, then shows the online user agreement (declined for the test).
- UNIVERSE menu: PLAY MATCHES, CALENDAR, RIVALRIES, STORY COLLECTION, WWE.com, **CUSTOMIZE UNIVERSE**. A new Universe already exists with a starting match card.
- WWE.com screen: LATEST NEWS, CURRENT RANKING per championship, **STATS** (WINS, APPEARANCES: "statistics for championships, shows, and more"): the stat tracking already exists in 2K15.
- CUSTOMIZE UNIVERSE: **EDIT CALENDAR** ("Create, edit, or delete shows and PPVs"), EDIT SUPERSTARS, **EDIT ROSTERS** ("Create or edit rosters to use in your shows"), EDIT TEAMS, **EDIT TITLES** ("Change or remove the Superstars who hold championships"), STORY OPTIONS, RESET UNIVERSE.
- EDIT ROSTERS: 12 roster slots (RAW, SmackDown, NXT, WWE, two more logos, INACTIVE, empty ones). The RAW roster screen shows a **ROSTER SIZE** panel: SUPERSTARS 31, DIVAS 8, TOTAL 39, **MINIMUM 8**, and the show's airing day. So the minimum is **computed per show/roster** (here 8), not a fixed 15 as in the WWE '12 notes.
- The UI texts (menus, errors) are not in the guest image range 0x82000000-0x83A00000 (they live in heap-loaded tables): the message text and the function computing MINIMUM are still to be located (next probe: a dump of the committed heap regions, or tracing which Univers functions run when the roster panel opens).
- Mods note: Universe UI flows are data (UI tool "Flow/Program" nodes in the menu pacs) calling native functions, so some rules may be changeable in data and not only in code.

## 9. Roster minimum and show validation (traced 2026-11-09)
- The **MINIMUM** line of the ROSTER SIZE panel is not a rule, only a display: panel formatter `sub_8284AA38` prints SUPERSTARS, DIVAS, TOTAL and MINIMUM = `min(superstars, divas)` (`cmpw r28,r29`); the counts come from `sub_82A5A3B8(roster, gender (2 = any), flag)`
  (loop over 0..499 roster slots). RAW 31/8 -> 8, SmackDown 18/1 -> 1. Callers: `82836DE0`, `828458A0`, `8284AA38`.
- A custom roster with **0 wrestlers is accepted** without any message (EDIT ROSTERS). No size check exists when creating a roster.
- SHOW EDIT (new Major show): fields SHOW NAME, SHOW LOGO, ROSTERS, ARENA, MATCH-UP SCREEN, 4 championship boxes (World/Singles/Tag/Diva), MATCHES (5), UNIVERSE DRAFT, ACCEPT.
  ACCEPT with an empty roster and no title shows the dialog "A World Championship must be chosen when creating a Major Show." (ATTENTION, OK). No roster-size message appeared first: the title check comes before.
  Major shows need a World title; Minor shows do not (type asked at creation).
- Tracing every function of 0x82A50000-0x82AA1000 during ACCEPT showed only per-frame UI helpers (`82A58630`, `82A84480`, `82A50098`, `82A599D0`, `82A5CA30`, `82A69168`, `82A51258`, `82A72830`, `82A8D4D8`): the show-edit validation therefore lives in the **UI module outside that range** (near the roster panel code `8283xxxx-8285xxxx`), not in the Universe simulation module.
- Consequence for the objectives: (a) "no minimum of 15" is likely only a UI/validation check in the 8283-8285 range (+ possibly the match-card generator needing enough wrestlers): still to find by tracing 0x82830000-0x82860000 while ACCEPTing a show with a roster of 1-2 wrestlers.

## 10. Show validator found (traced 2026-11-09, UI module 0x8283-0x8286)
- ACCEPT in SHOW EDIT = `sub_828484B0` (page switch via `sub_82844DC0`; case "accept" calls `sub_82844C28` = show name not empty, then, if the roster/host field at +5092 is set, the validator **`sub_82848228(editor)`**; it returns 0 and stores an error code at editor+4892 (4 = no World title for a Major show, 6 / 8 / 12 other cases) which selects the ATTENTION text. On failure ACCEPT offers "stop editing?").
- Validator logic (static read): show struct at editor+4904 (valid flag = bit 5 of byte +160); `82A73268`/`82A72858`/`82A72FA0` = major/minor/host tests (error 12 = Minor show without a usable host); `82A724F0(show,0) == 255` = no World title -> error 4;
  then it counts roster entries (editor+284, up to 6) that are valid, and for each of the match-slot groups (`6 + 4*n`, 12 groups, 4 entries each, `82A727F0` = slot in use, `82A72EE8`/`82A72830` = eligible entry) checks that **every used match slot can still be filled**; if not: error 6/8 "No matches can currently be held for this show".
  There is therefore **no fixed roster minimum (like 15)**: the rule is "each match type configured for the show must be fillable from the roster(s) (gender, count)". Removing/relaxing it = patch the final tests of `sub_82848228` (return 1) - easy, but the day loop (`sub_82A63708`) and card generator must cope with a show that cannot fill its matches (not verified).
- Minor shows: the ROSTERS box selects up to **2 Major shows as hosts** (their roster is shared); a Minor show with no host shows "You must have a Major Show that is able to host a Minor Show in order to create this show. No matches can currently be held for this show. Are you sure you want to stop editing?" (YES/NO).
- Defaults seen: Minor show = 3 matches, no championships; Major = 5 matches, 4 title slots. Roster "Legends" = 5 members, airing none.
- Not done: assigning a 1-2 wrestler roster to a show (roster picker selection did not register through keyboard taps); exact text of codes 6/8/12 not captured.
