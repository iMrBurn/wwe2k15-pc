# Mods for WWE 2K15 PC

Resume en francais a la fin / French summary at the end.

## How it works

The `mods` folder (next to `WWE2K15-PC.exe` / `wwe2k15.exe`) is laid over the game's files.
When the game opens a file, it first looks for the same relative path in `mods`; if it exists,
that file is used instead of the original. Your game folder or disc image is never modified, and
deleting a file from `mods` brings the original back. This works whether the game runs from a
folder or from an `.iso`, and it also applies to Title Update files (the game's `UPDATE:` files).
Mods win over everything else, including the files in `game_update`.

Changes are read when the game starts: restart the game after adding or removing a file.

## Where to put files

Use exactly the same path as in the game, starting inside `mods`:

| Game file | Put it here |
| --- | --- |
| `pac\menu\menu.pac` | `mods\pac\menu\menu.pac` |
| `movies\<name>.bk2` | `mods\movies\<name>.bk2` |
| a Title Update file, for example `pac\<something>.pac` from the TU | `mods\pac\<something>.pac` (same relative path as in the TU) |

Rules:
- Same folder names and same file name as the original. Spelling counts (case is ignored).
- Only whole files are replaced. Folders are not merged or created for you.
- The replacement must be in the same format as the original (a `.pac` stays a `.pac`, a `.bk2`
  stays a `.bk2`). A wrong or broken file can crash the game or show a black screen: remove it
  from `mods` to go back to normal.
- If the same relative path exists in both the base game and the Title Update, the one mod file
  replaces both.

## Turning mods on and off

In the launcher, open the Mods tab:
- **Enable mods** checkbox: off = the game ignores the whole `mods` folder.
- **Open mods folder** button: opens (and creates if needed) the folder.

Command line equivalent: `--wwe2k15_mods=false`.
To disable a single mod, move its file out of `mods`.

## File types that can be replaced

Any file the game reads by name can be replaced. The types found in the game are:
- `.pac` (archives: menus, textures, models, ...),
- `.bk2` (Bink 2 videos),
- `.xzp` (compressed packs), `.pck` (sound banks), `.arc`, `.def` and other data files.

Which of those give a usable result depends on the mod: this port does not repack or convert
anything, so the file must already be in the console format the game expects.
Sound banks and `.pac` files are the most commonly swapped; always test on a copy of your save.

## Never share game files

Do not post, upload or send game files (the original ones or the ones taken from the console),
the Title Update, DLC or the ISO. Share your mod only as the changed files that you made
yourself, or as instructions/tools to make them, and say which relative path each one goes to.

---

## Resume en francais

- Le dossier `mods` (a cote de l'exe) remplace les fichiers du jeu qui ont le meme chemin
  relatif. Ca marche avec un dossier de jeu comme avec une ISO, et aussi pour les fichiers de la
  Title Update. Les mods passent avant tout le reste. Le jeu original n'est jamais modifie.
- Exemples : `mods\pac\menu\menu.pac`, `mods\movies\<nom>.bk2`.
- Meme nom, meme dossier, meme format que l'original. Relance le jeu apres chaque changement.
- Dans le launcher : case "Activer les mods" et bouton "Ouvrir le dossier mods". Pour retirer un
  mod, enleve son fichier du dossier.
- Types connus : `.pac`, `.bk2`, `.xzp`, `.pck`, `.arc`, `.def`.
- Ne partage jamais de fichiers du jeu (jeu, TU, DLC, ISO) : seulement tes propres fichiers modifies.
