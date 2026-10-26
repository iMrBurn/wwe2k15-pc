// Launcher UI translations (generated). The key is the French text used in launcher.cpp.
#pragma once

#include <cstring>

namespace i18n {

// Languages offered, as Xbox 360 XLanguage ids passed to the game as user_language.
constexpr int kLanguageIds[] = {1, 3, 4, 5, 6};  // en, de, fr, es, it
inline const char* const kLanguageNames[] = {"English", "Deutsch", "Français", "Español", "Italiano"};
constexpr int kLanguageCount = 5;

inline const char* const kKeysAzerty = "Déplacement : Z Q S D    Stick droit : flèches\nA : Espace    B : Retour arrière    X : L    Y : P\nLB : 1    RB : 3    LT : A ou I    RT : E ou O    Croix directionnelle : Maj + flèches\nStart : Entrée    Back : Tab\n\nGarde chaque touche enfoncée un court instant : le jeu lit l'état des touches à chaque image.";
inline const char* const kKeysQwerty = "Déplacement : W A S D    Stick droit : flèches\nA : Espace    B : Retour arrière    X : L    Y : P\nLB : 1    RB : 3    LT : Q ou I    RT : E ou O    Croix directionnelle : Maj + flèches\nStart : Entrée    Back : Tab\n\nGarde chaque touche enfoncée un court instant : le jeu lit l'état des touches à chaque image.";

struct Entry { const char* fr; const char* en; const char* de; const char* es; const char* it; };
inline const Entry kEntries[] = {
  {"PRÉRÉGLAGE", "PRESET", "VOREINSTELLUNG", "PREAJUSTE", "PRESET"},
  {"Performance pour les petites configurations, Qualité pour les PC puissants.", "Performance for low-end PCs, Quality for powerful ones.", "Leistung für schwächere PCs, Qualität für starke PCs.", "Rendimiento para equipos modestos, Calidad para PC potentes.", "Prestazioni per PC modesti, Qualità per PC potenti."},
  {"PERFORMANCE", "PERFORMANCE", "LEISTUNG", "RENDIMIENTO", "PRESTAZIONI"},
  {"ÉQUILIBRÉ", "BALANCED", "AUSGEWOGEN", "EQUILIBRADO", "BILANCIATO"},
  {"QUALITÉ", "QUALITY", "QUALITÄT", "CALIDAD", "QUALITÀ"},
  {"RÉSOLUTION DE RENDU", "RENDER RESOLUTION", "RENDERAUFLÖSUNG", "RESOLUCIÓN DE RENDERIZADO", "RISOLUZIONE DI RENDERING"},
  {"1x = 720p comme sur Xbox 360. 2x et plus : plus net, bien plus lourd (expérimental).", "1x = 720p like on Xbox 360. 2x and above: sharper, much heavier (experimental).", "1x = 720p wie auf der Xbox 360. Ab 2x: schärfer, deutlich anspruchsvoller (experimentell).", "1x = 720p como en Xbox 360. 2x o más: más nítido, mucho más exigente (experimental).", "1x = 720p come su Xbox 360. Da 2x: più nitido, molto più pesante (sperimentale)."},
  {"ANTICRÉNELAGE", "ANTI-ALIASING", "KANTENGLÄTTUNG", "ANTIALIASING", "ANTIALIASING"},
  {"Adoucit les bords en escalier (FXAA).", "Smooths jagged edges (FXAA).", "Glättet gezackte Kanten (FXAA).", "Suaviza los bordes dentados (FXAA).", "Ammorbidisce i bordi seghettati (FXAA)."},
  {"AFFICHAGE", "DISPLAY", "ANZEIGE", "PANTALLA", "SCHERMO"},
  {"PLEIN ÉCRAN", "FULLSCREEN", "VOLLBILD", "PANTALLA COMPLETA", "SCHERMO INTERO"},
  {"FENÊTRE", "WINDOWED", "FENSTER", "VENTANA", "FINESTRA"},
  {"TAILLE DE LA FENÊTRE", "WINDOW SIZE", "FENSTERGRÖSSE", "TAMAÑO DE VENTANA", "DIMENSIONE FINESTRA"},
  {"ÉCRAN", "MONITOR", "BILDSCHIRM", "MONITOR", "MONITOR"},
  {"AUTO", "AUTO", "AUTO", "AUTO", "AUTO"},
  {"Bandes noires (garder le format 16:9)", "Black bars (keep 16:9)", "Schwarze Balken (16:9 beibehalten)", "Bandas negras (mantener 16:9)", "Bande nere (mantieni il 16:9)"},
  {"Corrige les écrans noirs des entrées (NVIDIA, Intel).", "Fixes black screens during entrances (NVIDIA, Intel).", "Behebt schwarze Bilder bei den Einzügen (NVIDIA, Intel).", "Corrige las pantallas negras de las entradas (NVIDIA, Intel).", "Corregge le schermate nere degli ingressi (NVIDIA, Intel)."},
  {"Afficher le compteur d'images/s", "Show FPS counter", "FPS-Anzeige einblenden", "Mostrar contador de FPS", "Mostra contatore FPS"},
  {"Dossier du jeu et options de lancement", "Game folder and launch options", "Spielordner und Startoptionen", "Carpeta del juego y opciones de inicio", "Cartella del gioco e opzioni di avvio"},
  {"JOUER", "PLAY", "SPIELEN", "JUGAR", "GIOCA"},
  {"RÉGLAGES", "SETTINGS", "EINSTELLUNGEN", "AJUSTES", "IMPOSTAZIONI"},
  {"GRAPHISMES", "GRAPHICS", "GRAFIK", "GRÁFICOS", "GRAFICA"},
  {"CONTRÔLES", "CONTROLS", "STEUERUNG", "CONTROLES", "COMANDI"},
  {"SAUVEGARDES", "SAVES", "SPIELSTÄNDE", "PARTIDAS", "SALVATAGGI"},
  {"À PROPOS", "ABOUT", "INFO", "ACERCA DE", "INFO"},
  {"Sélectionne d'abord le dossier qui contient default.xex.", "Select the folder that contains default.xex first.", "Wähle zuerst den Ordner mit default.xex.", "Primero selecciona la carpeta que contiene default.xex.", "Seleziona prima la cartella che contiene default.xex."},
  {"Choisis la Title Update (tu0000...) et/ou les DLC de WWE 2K15", "Choose the WWE 2K15 Title Update (tu0000...) and/or DLC", "Wähle das Title Update (tu0000...) und/oder DLC von WWE 2K15", "Elige la Title Update (tu0000...) y/o los DLC de WWE 2K15", "Scegli il Title Update (tu0000...) e/o i DLC di WWE 2K15"},
  {"Choisis le dossier contenant default.xex", "Choose the folder containing default.xex", "Wähle den Ordner mit default.xex", "Elige la carpeta que contiene default.xex", "Scegli la cartella che contiene default.xex"},
  {"Dossier du jeu enregistré.", "Game folder saved.", "Spielordner gespeichert.", "Carpeta del juego guardada.", "Cartella del gioco salvata."},
  {"Ce dossier ne contient pas default.xex.", "This folder does not contain default.xex.", "Dieser Ordner enthält keine default.xex.", "Esta carpeta no contiene default.xex.", "Questa cartella non contiene default.xex."},
  {"Le jeu tourne déjà.", "The game is already running.", "Das Spiel läuft bereits.", "El juego ya se está ejecutando.", "Il gioco è già in esecuzione."},
  {"wwe2k15.exe est introuvable : reconstruis le projet.", "wwe2k15.exe not found: rebuild the project.", "wwe2k15.exe nicht gefunden: Projekt neu bauen.", "No se encuentra wwe2k15.exe: recompila el proyecto.", "wwe2k15.exe non trovato: ricompila il progetto."},
  {"Sélectionne le dossier qui contient default.xex.", "Select the folder that contains default.xex.", "Wähle den Ordner mit default.xex.", "Selecciona la carpeta que contiene default.xex.", "Seleziona la cartella che contiene default.xex."},
  {"Ajoute la Title Update 4 (RÉGLAGES > AJOUTER TU / DLC).", "Add Title Update 4 (SETTINGS > ADD TU / DLC).", "Füge Title Update 4 hinzu (EINSTELLUNGEN > TU / DLC HINZUFÜGEN).", "Añade la Title Update 4 (AJUSTES > AÑADIR TU / DLC).", "Aggiungi il Title Update 4 (IMPOSTAZIONI > AGGIUNGI TU / DLC)."},
  {"Jeu en cours de démarrage…", "Starting the game…", "Spiel wird gestartet…", "Iniciando el juego…", "Avvio del gioco…"},
  {"Commande de lancement envoyée.", "Launch command sent.", "Startbefehl gesendet.", "Orden de inicio enviada.", "Comando di avvio inviato."},
  {"Impossible de régler l'option du hook OBS Vulkan.", "Could not set the OBS Vulkan hook option.", "Die OBS-Vulkan-Hook-Option konnte nicht gesetzt werden.", "No se pudo ajustar la opción del hook Vulkan de OBS.", "Impossibile impostare l'opzione dell'hook Vulkan di OBS."},
  {"Windows n'a pas pu démarrer le jeu.", "Windows could not start the game.", "Windows konnte das Spiel nicht starten.", "Windows no pudo iniciar el juego.", "Windows non è riuscito ad avviare il gioco."},
  {"Jeu en cours d'exécution.", "Game running.", "Spiel läuft.", "Juego en ejecución.", "Gioco in esecuzione."},
  {"Le jeu s'est arrêté (code ", "The game stopped (code ", "Das Spiel wurde beendet (Code ", "El juego se detuvo (código ", "Il gioco si è chiuso (codice "},
  {"). Consulte JOURNAUX.", "). See LOGS.", "). Siehe PROTOKOLLE.", "). Consulta REGISTROS.", "). Vedi LOG."},
  {"PC  ·  RECOMPILATION NATIVE", "PC  ·  NATIVE RECOMPILATION", "PC  ·  NATIVE REKOMPILIERUNG", "PC  ·  RECOMPILACIÓN NATIVA", "PC  ·  RICOMPILAZIONE NATIVA"},
  {"Changer d'onglet", "Switch tab", "Tab wechseln", "Cambiar pestaña", "Cambia scheda"},
  {"ÉCHAP / B", "ESC / B", "ESC / B", "ESC / B", "ESC / B"},
  {"Retour à Jouer", "Back to Play", "Zurück zu Spielen", "Volver a Jugar", "Torna a Gioca"},
  {"Beta de développement", "Development beta", "Entwicklungs-Beta", "Beta de desarrollo", "Beta di sviluppo"},
  {"Version PC non officielle · rendu Vulkan · manette SDL", "Unofficial PC version · Vulkan rendering · SDL controller", "Inoffizielle PC-Version · Vulkan-Rendering · SDL-Controller", "Versión de PC no oficial · renderizado Vulkan · mando SDL", "Versione PC non ufficiale · rendering Vulkan · controller SDL"},
  {"FICHIERS DU JEU", "GAME FILES", "SPIELDATEIEN", "ARCHIVOS DEL JUEGO", "FILE DEL GIOCO"},
  {"TROUVÉS", "FOUND", "GEFUNDEN", "ENCONTRADOS", "TROVATI"},
  {"À SÉLECTIONNER", "NOT SELECTED", "NICHT GEWÄHLT", "SIN SELECCIONAR", "DA SELEZIONARE"},
  {"RENDU", "RENDERER", "RENDERER", "RENDERIZADO", "RENDERING"},
  {"MANETTE", "CONTROLLER", "CONTROLLER", "MANDO", "CONTROLLER"},
  {"AUCUNE DÉTECTÉE", "NONE DETECTED", "KEINER ERKANNT", "NINGUNO DETECTADO", "NESSUNO RILEVATO"},
  {"TU4 INSTALLÉE", "TU4 INSTALLED", "TU4 INSTALLIERT", "TU4 INSTALADA", "TU4 INSTALLATO"},
  {"MANQUANTE", "MISSING", "FEHLT", "FALTA", "MANCANTE"},
  {"AUCUN", "NONE", "KEINE", "NINGUNO", "NESSUNO"},
  {"EN COURS…", "RUNNING…", "LÄUFT…", "EN CURSO…", "IN CORSO…"},
  {"CHOISIR LE JEU", "CHOOSE GAME", "SPIEL WÄHLEN", "ELEGIR JUEGO", "SCEGLI IL GIOCO"},
  {"JOURNAUX", "LOGS", "PROTOKOLLE", "REGISTROS", "LOG"},
  {"Aucune donnée du jeu n'est incluse. Une copie légale du jeu est requise.", "No game data is included. A legal copy of the game is required.", "Es sind keine Spieldaten enthalten. Eine legale Kopie des Spiels wird benötigt.", "No se incluyen datos del juego. Se necesita una copia legal del juego.", "Non è incluso alcun dato del gioco. È necessaria una copia legale del gioco."},
  {"DOSSIER DU JEU XBOX 360 (doit contenir default.xex)", "XBOX 360 GAME FOLDER (must contain default.xex)", "XBOX-360-SPIELORDNER (muss default.xex enthalten)", "CARPETA DEL JUEGO DE XBOX 360 (debe contener default.xex)", "CARTELLA DEL GIOCO XBOX 360 (deve contenere default.xex)"},
  {"Aucun dossier sélectionné", "No folder selected", "Kein Ordner gewählt", "Ninguna carpeta seleccionada", "Nessuna cartella selezionata"},
  {"PARCOURIR…", "BROWSE…", "DURCHSUCHEN…", "EXAMINAR…", "SFOGLIA…"},
  {"ENREGISTRER", "SAVE", "SPEICHERN", "GUARDAR", "SALVA"},
  {"Réglages enregistrés.", "Settings saved.", "Einstellungen gespeichert.", "Ajustes guardados.", "Impostazioni salvate."},
  {"AJOUTER TU / DLC…", "ADD TU / DLC…", "TU / DLC HINZUFÜGEN…", "AÑADIR TU / DLC…", "AGGIUNGI TU / DLC…"},
  {"Désactiver le hook Vulkan d'OBS", "Disable the OBS Vulkan hook", "OBS-Vulkan-Hook deaktivieren", "Desactivar el hook Vulkan de OBS", "Disattiva l'hook Vulkan di OBS"},
  {"Activé : évite l'injection d'OBS dans le rendu Vulkan du jeu.", "On: keeps OBS out of the game's Vulkan rendering.", "An: verhindert, dass sich OBS in das Vulkan-Rendering einklinkt.", "Activado: evita que OBS se inyecte en el renderizado Vulkan del juego.", "Attivo: impedisce a OBS di inserirsi nel rendering Vulkan del gioco."},
  {"Désactivé : OBS peut capturer le jeu via son hook Vulkan.", "Off: OBS can capture the game through its Vulkan hook.", "Aus: OBS kann das Spiel über seinen Vulkan-Hook aufnehmen.", "Desactivado: OBS puede capturar el juego con su hook Vulkan.", "Disattivo: OBS può catturare il gioco tramite il suo hook Vulkan."},
  {"Rendu : Vulkan (GPU haute performance)  ·  Graphismes et clavier : onglets GRAPHISMES et CONTRÔLES", "Renderer: Vulkan (high-performance GPU)  ·  Graphics and keyboard: GRAPHICS and CONTROLS tabs", "Renderer: Vulkan (Hochleistungs-GPU)  ·  Grafik und Tastatur: Tabs GRAFIK und STEUERUNG", "Renderizado: Vulkan (GPU de alto rendimiento)  ·  Gráficos y teclado: pestañas GRÁFICOS y CONTROLES", "Rendering: Vulkan (GPU ad alte prestazioni)  ·  Grafica e tastiera: schede GRAFICA e COMANDI"},
  {"AJOUTER TU / DLC : la Title Update 4 (tu00000004_00000000) est requise ; les DLC (545408B6\\00000002) sont optionnels.", "ADD TU / DLC: Title Update 4 (tu00000004_00000000) is required; DLC (545408B6\\00000002) is optional.", "TU / DLC HINZUFÜGEN: Title Update 4 (tu00000004_00000000) ist erforderlich; DLC (545408B6\\00000002) sind optional.", "AÑADIR TU / DLC: la Title Update 4 (tu00000004_00000000) es obligatoria; los DLC (545408B6\\00000002) son opcionales.", "AGGIUNGI TU / DLC: il Title Update 4 (tu00000004_00000000) è obbligatorio; i DLC (545408B6\\00000002) sono facoltativi."},
  {"Appliqués au prochain lancement du jeu", "Applied the next time the game starts", "Gilt ab dem nächsten Spielstart", "Se aplican la próxima vez que inicies el juego", "Applicate al prossimo avvio del gioco"},
  {"RÉSOLUTION INTERNE", "INTERNAL RESOLUTION", "INTERNE AUFLÖSUNG", "RESOLUCIÓN INTERNA", "RISOLUZIONE INTERNA"},
  {"1x = 720p comme sur Xbox 360. 2x et 3x sont plus nets mais bien plus lourds (expérimental).", "1x = 720p like on Xbox 360. 2x and 3x are sharper but much heavier (experimental).", "1x = 720p wie auf der Xbox 360. 2x und 3x sind schärfer, aber deutlich anspruchsvoller (experimentell).", "1x = 720p como en Xbox 360. 2x y 3x son más nítidos pero mucho más exigentes (experimental).", "1x = 720p come su Xbox 360. 2x e 3x sono più nitidi ma molto più pesanti (sperimentale)."},
  {"FILTRAGE DES TEXTURES", "TEXTURE FILTERING", "TEXTURFILTERUNG", "FILTRADO DE TEXTURAS", "FILTRO DELLE TEXTURE"},
  {"Rend le sol et le public plus nets de loin.", "Keeps the floor and the crowd sharp at a distance.", "Hält Boden und Publikum auch aus der Ferne scharf.", "Mantiene nítidos el suelo y el público a distancia.", "Rende più nitidi il pavimento e il pubblico da lontano."},
  {"Plein écran", "Fullscreen", "Vollbild", "Pantalla completa", "Schermo intero"},
  {"Rendu précis (FSI)", "Accurate rendering (FSI)", "Genaues Rendering (FSI)", "Renderizado preciso (FSI)", "Rendering accurato (FSI)"},
  {"Recommandé : corrige les écrans noirs pendant les entrées. Nécessite une carte NVIDIA ou Intel récente.", "Recommended: fixes black screens during entrances. Needs a recent NVIDIA or Intel GPU.", "Empfohlen: behebt schwarze Bilder bei den Einzügen. Benötigt eine aktuelle NVIDIA- oder Intel-GPU.", "Recomendado: corrige las pantallas negras durante las entradas. Requiere una GPU NVIDIA o Intel reciente.", "Consigliato: corregge le schermate nere durante gli ingressi. Richiede una GPU NVIDIA o Intel recente."},
  {"Réduire l'utilisation du processeur (expérimental)", "Reduce CPU usage (experimental)", "CPU-Auslastung senken (experimentell)", "Reducir el uso de CPU (experimental)", "Riduci l'uso della CPU (sperimentale)"},
  {"Laisse le fil de rendu se reposer pendant qu'il attend la carte graphique. Peut changer la fluidité.", "Lets the render thread rest while it waits for the GPU. May affect smoothness.", "Lässt den Render-Thread ruhen, während er auf die GPU wartet. Kann die Flüssigkeit beeinflussen.", "Deja descansar el hilo de renderizado mientras espera a la GPU. Puede afectar a la fluidez.", "Lascia riposare il thread di rendering mentre attende la GPU. Può influire sulla fluidità."},
  {"Manette ou clavier", "Controller or keyboard", "Controller oder Tastatur", "Mando o teclado", "Controller o tastiera"},
  {"Branche ta manette avant de lancer le jeu. SDL gère les manettes Xbox et PlayStation, dont la DualSense.", "Plug in your controller before starting the game. SDL supports Xbox and PlayStation controllers, including the DualSense.", "Schließe deinen Controller vor dem Spielstart an. SDL unterstützt Xbox- und PlayStation-Controller, auch den DualSense.", "Conecta tu mando antes de iniciar el juego. SDL admite mandos de Xbox y PlayStation, incluido el DualSense.", "Collega il controller prima di avviare il gioco. SDL supporta i controller Xbox e PlayStation, compreso il DualSense."},
  {"Jouer au clavier (manette virtuelle)", "Play with the keyboard (virtual controller)", "Mit der Tastatur spielen (virtueller Controller)", "Jugar con el teclado (mando virtual)", "Gioca con la tastiera (controller virtuale)"},
  {"Clavier AZERTY (français)", "AZERTY keyboard (French)", "AZERTY-Tastatur (Französisch)", "Teclado AZERTY (francés)", "Tastiera AZERTY (francese)"},
  {"Préparation", "Coming soon", "In Vorbereitung", "En preparación", "In preparazione"},
  {"Espace de préparation des mods.\nLe remplacement de fichiers n'est pas encore activé dans cette beta.", "Mod staging area.\nFile replacement is not enabled yet in this beta.", "Bereich für Mods.\nDas Ersetzen von Dateien ist in dieser Beta noch nicht aktiv.", "Zona de preparación de mods.\nEl reemplazo de archivos aún no está activado en esta beta.", "Area di preparazione delle mod.\nLa sostituzione dei file non è ancora attiva in questa beta."},
  {"OUVRIR LE DOSSIER MODS", "OPEN MODS FOLDER", "MODS-ORDNER ÖFFNEN", "ABRIR CARPETA DE MODS", "APRI CARTELLA MOD"},
  {"Données utilisateur", "User data", "Benutzerdaten", "Datos de usuario", "Dati utente"},
  {"Sauvegardes et données utilisateur stockées dans\nLocalAppData\\WWE2K15.", "Saves and user data are stored in\nLocalAppData\\WWE2K15.", "Spielstände und Benutzerdaten liegen in\nLocalAppData\\WWE2K15.", "Las partidas y los datos de usuario se guardan en\nLocalAppData\\WWE2K15.", "Salvataggi e dati utente si trovano in\nLocalAppData\\WWE2K15."},
  {"OUVRIR LE DOSSIER", "OPEN FOLDER", "ORDNER ÖFFNEN", "ABRIR CARPETA", "APRI CARTELLA"},
  {"Portage PC non officiel de WWE 2K15, basé sur ReXGlue.\nAucune donnée du jeu n'est fournie.\n\nSans affiliation avec WWE, 2K ou Yuke's.", "Unofficial PC port of WWE 2K15, based on ReXGlue.\nNo game data is included.\n\nNot affiliated with WWE, 2K or Yuke's.", "Inoffizielle PC-Portierung von WWE 2K15 auf Basis von ReXGlue.\nEs sind keine Spieldaten enthalten.\n\nKeine Verbindung zu WWE, 2K oder Yuke's.", "Port no oficial de WWE 2K15 para PC, basado en ReXGlue.\nNo se incluye ningún dato del juego.\n\nSin afiliación con WWE, 2K ni Yuke's.", "Port PC non ufficiale di WWE 2K15, basato su ReXGlue.\nNon è incluso alcun dato del gioco.\n\nNon affiliato a WWE, 2K o Yuke's."},
  {"DÉPÔT GITHUB", "GITHUB REPOSITORY", "GITHUB-REPOSITORY", "REPOSITORIO DE GITHUB", "REPOSITORY GITHUB"},
  {" : pas un package Xbox 360 (STFS) lisible.", ": not a readable Xbox 360 (STFS) package.", ": kein lesbares Xbox-360-Paket (STFS).", ": no es un paquete de Xbox 360 (STFS) legible.", ": non è un pacchetto Xbox 360 (STFS) leggibile."},
  {" : ce package n'est pas pour WWE 2K15.", ": this package is not for WWE 2K15.", ": dieses Paket ist nicht für WWE 2K15.", ": este paquete no es de WWE 2K15.", ": questo pacchetto non è per WWE 2K15."},
  {" : copie impossible (", ": copy failed (", ": Kopieren fehlgeschlagen (", ": no se pudo copiar (", ": copia non riuscita ("},
  {"DLC ajouté : ", "DLC added: ", "DLC hinzugefügt: ", "DLC añadido: ", "DLC aggiunto: "},
  {" : type de package non pris en charge.", ": unsupported package type.", ": nicht unterstützter Pakettyp.", ": tipo de paquete no compatible.", ": tipo di pacchetto non supportato."},
  {" : seule la Title Update 4 est prise en charge.", ": only Title Update 4 is supported.", ": nur Title Update 4 wird unterstützt.", ": solo se admite la Title Update 4.", ": è supportato solo il Title Update 4."},
  {" : extraction échouée pour ", ": extraction failed for ", ": Entpacken fehlgeschlagen für ", ": error al extraer ", ": estrazione non riuscita per "},
  {"Title Update 4 installée.", "Title Update 4 installed.", "Title Update 4 installiert.", "Title Update 4 instalada.", "Title Update 4 installato."},
  {" package(s) installé(s). Dernier : ", " package(s) installed. Last: ", " Paket(e) installiert. Zuletzt: ", " paquete(s) instalado(s). Último: ", " pacchetto/i installato/i. Ultimo: "},
  {"LANGUE", "LANGUAGE", "SPRACHE", "IDIOMA", "LINGUA"},
  {"Langue du jeu et de ce lanceur.", "Language of the game and of this launcher.", "Sprache des Spiels und dieses Launchers.", "Idioma del juego y de este launcher.", "Lingua del gioco e di questo launcher."},
  {"Déplacement : Z Q S D    Stick droit : flèches\nA : Espace    B : Retour arrière    X : L    Y : P\nLB : 1    RB : 3    LT : A ou I    RT : E ou O    Croix directionnelle : Maj + flèches\nStart : Entrée    Back : Tab\n\nGarde chaque touche enfoncée un court instant : le jeu lit l'état des touches à chaque image.", "Movement: Z Q S D    Right stick: arrow keys\nA: Space    B: Backspace    X: L    Y: P\nLB: 1    RB: 3    LT: A or I    RT: E or O    D-pad: Shift + arrow keys\nStart: Enter    Back: Tab\n\nHold each key briefly: the game reads the key state once per frame.", "Bewegung: Z Q S D    Rechter Stick: Pfeiltasten\nA: Leertaste    B: Rücktaste    X: L    Y: P\nLB: 1    RB: 3    LT: A oder I    RT: E oder O    Steuerkreuz: Umschalt + Pfeiltasten\nStart: Eingabe    Back: Tab\n\nHalte jede Taste kurz gedrückt: Das Spiel liest den Tastenzustand einmal pro Bild.", "Movimiento: Z Q S D    Stick derecho: flechas\nA: Espacio    B: Retroceso    X: L    Y: P\nLB: 1    RB: 3    LT: A o I    RT: E o O    Cruceta: Mayús + flechas\nStart: Intro    Back: Tab\n\nMantén cada tecla pulsada un instante: el juego lee el estado de las teclas en cada fotograma.", "Movimento: Z Q S D    Stick destro: frecce\nA: Spazio    B: Backspace    X: L    Y: P\nLB: 1    RB: 3    LT: A o I    RT: E o O    Croce direzionale: Maiusc + frecce\nStart: Invio    Back: Tab\n\nTieni premuto ogni tasto per un attimo: il gioco legge lo stato dei tasti a ogni fotogramma."},
  {"Déplacement : W A S D    Stick droit : flèches\nA : Espace    B : Retour arrière    X : L    Y : P\nLB : 1    RB : 3    LT : Q ou I    RT : E ou O    Croix directionnelle : Maj + flèches\nStart : Entrée    Back : Tab\n\nGarde chaque touche enfoncée un court instant : le jeu lit l'état des touches à chaque image.", "Movement: W A S D    Right stick: arrow keys\nA: Space    B: Backspace    X: L    Y: P\nLB: 1    RB: 3    LT: Q or I    RT: E or O    D-pad: Shift + arrow keys\nStart: Enter    Back: Tab\n\nHold each key briefly: the game reads the key state once per frame.", "Bewegung: W A S D    Rechter Stick: Pfeiltasten\nA: Leertaste    B: Rücktaste    X: L    Y: P\nLB: 1    RB: 3    LT: Q oder I    RT: E oder O    Steuerkreuz: Umschalt + Pfeiltasten\nStart: Eingabe    Back: Tab\n\nHalte jede Taste kurz gedrückt: Das Spiel liest den Tastenzustand einmal pro Bild.", "Movimiento: W A S D    Stick derecho: flechas\nA: Espacio    B: Retroceso    X: L    Y: P\nLB: 1    RB: 3    LT: Q o I    RT: E o O    Cruceta: Mayús + flechas\nStart: Intro    Back: Tab\n\nMantén cada tecla pulsada un instante: el juego lee el estado de las teclas en cada fotograma.", "Movimento: W A S D    Stick destro: frecce\nA: Spazio    B: Backspace    X: L    Y: P\nLB: 1    RB: 3    LT: Q o I    RT: E o O    Croce direzionale: Maiusc + frecce\nStart: Invio    Back: Tab\n\nTieni premuto ogni tasto per un attimo: il gioco legge lo stato dei tasti a ogni fotogramma."},
};

inline const char* Translate(const char* fr, int language) {
  if (language == 4) return fr;
  for (const Entry& e : kEntries) {
    if (std::strcmp(e.fr, fr) == 0) {
      switch (language) {
        case 3: return e.de;
        case 5: return e.es;
        case 6: return e.it;
        default: return e.en;
      }
    }
  }
  return fr;
}

}  // namespace i18n
