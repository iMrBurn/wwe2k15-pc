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
