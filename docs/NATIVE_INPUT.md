# Entrées natives — livraison partielle, v1.1.1

Cette branche corrige le branchement manette et remplace l'accumulation des
mouvements souris par une conversion absolue dans la vue native. **Elle ne rend
pas les marges 32:9 interactives.** La souris reste expérimentale, activable par
F3 ; elle n'est pas activée par défaut. Ce n'est pas une livraison du correctif
complet de surface de jeu 32:9.

## Causes et modifications

- Le launcher imposait `io.player_src[0] = 1` et ne récupérait ni la source,
  ni le GUID, ni la deadzone après Play. Ces champs sont maintenant chargés et
  sauvegardés dans une section additive `[Input]` de `sc-settings.ini`.
  Les fichiers existants gardent Keyboard comme choix par défaut.
- Le jeu ne lisait que `ScKeybindsRead()`. `ScGamepad` ouvre maintenant un
  contrôleur SDL standard, lit ses boutons et axes et produit les bits série
  SNES attendus. SDL assure la prise en charge native des contrôleurs connus,
  dont DualSense ; aucun middleware externe n'est ajouté.
- Le GUID sélectionné filtre les périphériques ; une déconnexion libère le
  handle et renvoie zéro. Une reconnexion est recherchée avec le même GUID,
  sans réutiliser un identifiant d'instance périmé. USB et Bluetooth peuvent
  avoir des GUID différents : sélectionner à nouveau la connexion voulue dans
  le launcher dans ce cas. Deux manettes de même GUID ne sont pas distinguées.
- Le **launcher épinglé** écrit les mappings SNES dans `config.ini`, section
  `[GamepadMap]`, clé `Controls`, dans l'ordre
  `Up,Down,Left,Right,Select,Start,A,B,X,Y,L,R`. Les profils sauvegardés sont dans
  `[Controller.<GUID>]` (`Controls`, `Deadzone`). Ce n'est pas `input.ini` à cette
  révision, même si un autre bridge existe dans le sous-module.
  Le lecteur applique les valeurs globales puis le profil correspondant.
  Les entrées vides restent non assignées, sans décaler les suivantes.
  La configuration n'est jamais réécrite par le lecteur.
- Les boutons, clics de sticks, L1/R1 et gâchettes L2/R2 utilisent le vocabulaire
  de capture du launcher. Le stick gauche est un second D-pad avec la deadzone
  configurée et un plancher anti-bruit de 8000/32767. Les gâchettes agissent si
  elles sont assignées à un bouton SNES dans le launcher.
- Keyboard continue de fonctionner, y compris comme secours lorsque Gamepad
  est sélectionné. Keyboard seul n'active pas les entrées manette. Les entrées
  live sont neutralisées en l'absence de focus et dans les overlays hôte.
- La souris n'utilise plus `SDL_GetRelativeMouseState()` ni de reste fractionnel
  pour pointer. Ses coordonnées fenêtre sont converties en pixels drawable
  (DPI), puis par la destination affichée et l'ancrage natif effectivement
  utilisé par le renderer. Une souris immobile ne reprend pas le curseur après
  une entrée clavier/manette. Aucun déplacement du pointeur Windows n'est fait.
  Le pointage ne modifie la WRAM que sur la carte native, sans page advisor.
- En mode F3, un point dans une marge non prise en charge ne tronque pas X en
  octet et ne déclenche pas B/A sur une case native différente. Le glissement
  droit conserve le pan coordonné du jeu avec A + direction ; seul ce geste
  utilise un reste relatif, effacé à la fin du geste/perte de focus.
  Le réglage de vitesse souris est désormais nommé `MOUSE PAN SPEED` : une
  vitesse arbitraire de pointage absolu détruirait la correspondance géométrique.

Ces changements ne touchent pas au renderer, à la ROM, aux submodules,
à la simulation, à l'audio ou aux formats SRAM/save states.

## Pourquoi la surface 32:9 reste non interactive

`ScVideoToGuest()` n'avait aucun appel dans le chemin d'exécution souris de
`main.c`. Sa limite 256 × 224 décrit la **vue native**, pas toute la surface du
renderer. Elle est maintenant utilisée pour refuser une écriture hors de cette
vue. Changer son booléen ne modifie aucune interaction du jeu.

`apply_mouse_delta()` reste le déplacement expérimental F9, avec des bornes
natives. Il n'est plus utilisé pour le pointage souris absolu. Élargir son clamp
ou écrire X > 255 dans `$01eb` n'apporterait pas la surface demandée.
Les notes existantes montrent des accès 16 bits aux paires `$01eb/$01ec` et
`$01ed/$01ee`, **mais aussi des masques `& 0xff` et des bornes natives** : la
largeur des emplacements WRAM n'est pas, à elle seule, celle du domaine accepté.

Les composants distincts sont :

| Composant | Éléments identifiés | État |
| --- | --- | --- |
| Déplacement | `01:c0dd`, `01:c1ca`, `01:c214` | Bornes natives ; Y documenté entre 0x18 et 0xd0 selon le chemin |
| Scrolling aux bords | `01:c434`, `01:afbe`, `01:a0c4` | Seuils X/Y natifs, caméra `$01bd/$01bf`, limites `$01c5..$01cb` |
| Cible / interaction | `01:c616` et chemins de chaque outil | Chaîne complète non établie par les documents ; nécessite une trace ROM |
| Curseur / sprites | paramètres `$025d/$025f`, dispatch `01:c63e` | Déplacer seulement les sprites ne change pas la cible de construction |
| Terrain hôte | `host_map_compose()`, `ScMapView`, `ScRenderer` | Dessine le monde au-delà de la vue native, sans y adapter les outils |

Avec le renderer adaptatif activé, c'est `ScRenderer` qui compose l'extension,
indépendamment du chemin legacy `host_map_compose()`.
`host_map_screen_live()` sert encore de garde mais inclut certains fonds de
pages ; la garde souris exige en plus BG2 sur MAIN et exclut l'advisor adaptatif.

Le renderer lit un monde de 120 × 100 cellules à `$7F0200` (offset WRAM
`0x10200`), à huit pixels par cellule. Son origine de rendu ne se réduit pas à
`$01bd * 8` : elle inclut le scroll fin PPU, une correction de retard coarse/fine
et les états de chargement où l'ancienne carte est retenue. Dans `render_row()`,
le terrain utilise `sx + (canvas_x - core_x)` et `sy + y + 1`, plus un autre
échantillon pour les toits. Le `+1` d'alignement visuel n'est pas une preuve du
bon arrondi pour choisir une case constructible.

L'asymétrie basse des captures n'est donc pas attribuée à une constante au
hasard. Les clamps, les modes main/pan/outils et l'ancrage du sprite doivent
être observés au même moment de consommation d'entrée. Elle reste à résoudre.

## Architecture minimale viable pour terminer

1. Ajouter un état de curseur logique hôte (coordonnées étendues, propriétaire
   souris/clavier/manette, état drag et validité), distinct des octets natifs.
   Le clavier et le stick alimentent ce même état sur la carte seulement ; les
   menus continuent de recevoir le pad natif.
2. Exposer depuis `ScRenderer` la transformation de **la carte présentée**, avec
   origine coarse/fine corrigée, ancrage, état de transition et validité. En faire
   une paire de conversions canvas ↔ monde testable. Partager cette source avec
   les interactions, plutôt que recalculer un scroll concurrent.
3. Tracer avec une ROM personnelle vérifiée le calcul de cible utilisé par
   sélection, route, bâtiment, zonage, bulldozer et inspection. Observer les
   lectures de `$01eb/$01ed`, les temporaires et le point exact où chaque outil
   consomme sa cible. Les routines sont spécifiques à la région ROM ; un hook
   doit être conditionné à une empreinte connue et vérifier son contexte.
4. Traduire le curseur logique en **case monde au point de consommation** de
   l'outil, en conservant la validation, le coût, les règles de voisinage et les
   effets du jeu. Ne pas écrire directement les cellules du monde et ne pas
   déplacer temporairement la caméra pour simuler une interaction hors champ.
5. Séparer les limites du curseur logique, les limites monde et la demande de
   pan. Neutraliser le scrolling aux anciens seuils seulement dans ce contexte,
   puis déclencher le scrolling coordonné du jeu aux bords du nouveau viewport.
   Le pan ne doit pas être déclenché par une simple souris immobile.
6. Déplacer/remplacer le curseur natif à partir de la même cible validée, avec
   son vrai sprite/empreinte d'outil. Faire fonctionner la cible d'abord : un
   overlay visuel seul ne constitue pas une solution.
7. Invalider cet état hôte au chargement d'état, changement de carte/page/focus,
   resize ou changement de propriétaire. Préserver le format des sauvegardes.
   Activer la souris par défaut seulement après les validations ci-dessous.

Aucune ROM ni save state jouable n'était fourni dans cet environnement. Les
notes existantes ne décrivent pas tous les consommateurs de cible des outils.
Implémenter ces hooks sans les mesurer serait une modification du moteur
non qualifiée, susceptible de construire sur une autre case ou de corrompre
la carte. L'architecture ci-dessus est une suite de travail, pas du code livré.

## Validation effectuée

- Linux SDL2 : compilation et les trois CTests passent.
- Linux SDL3 3.2.10 Release : compilation et les trois CTests passent.
- Windows x64 Release SDL3 3.2.10 : compilation croisée MinGW-w64 réussie.
  Le binaire Windows n'a pas été exécuté sous Windows.
- `simcity_input` : chargement/sauvegarde source/GUID/deadzone, valeurs par
  défaut, mapping global/profil, ordre et entrées vides ; manette virtuelle SDL
  pour boutons, croix, stick, gâchettes et déconnexion/reconnexion par GUID.
- `simcity_video` : conversion aux seuils 255/256, 223/224, marge droite 683,
  ancrages centrés/non centrés, entrée drawable avec facteur DPI 150 %, viewport
  nul, et tests existants des ratios/Mods/persistance.
- `simcity_renderer` : suite de régression ROM-free existante ; aucun changement
  des sources de rendu. Les pins des deux submodules restent inchangés.

**Pas de validation physique** de DualSense, clic, construction, Alt+Tab,
redimensionnement, reprise de save state, ni de comparaison visuelle en jeu.
Les tests mathématiques ne prouvent pas la synchronisation du sprite natif.

## Recette Windows restant à effectuer

| # | Test demandé | Résultat à obtenir / état actuel |
| --- | --- | --- |
| 1 | DualSense dans le launcher | Tester USB puis Bluetooth avec SDL, sans middleware |
| 2 | Player 1 persistant | Choisir Gamepad, Play, fermer, relancer ; vérifier source/GUID et mappings |
| 3 | Menus et jeu manette | Tester les 12 boutons, stick, gâchettes assignées, débrancher/rebrancher pendant un maintien |
| 4 | Clavier | Vérifier les mappings existants et le secours clavier en mode Gamepad |
| 5 | Souris sans F3 | **Non livré** : toujours opt-in |
| 6 | Synchronisation 32:9 complète | **Non livré** : seule la conversion native a changé ; vérifier F3 dans cette zone |
| 7 | Quatre coins visibles | **Non livré** au-delà de la vue native |
| 8 | Construction/sélection dans les marges | **Non livré** ; en F3, vérifier que cliquer une marge n'agit pas sur une autre case native |
| 9 | Scrolling aux nouvelles limites | **Non livré** ; vérifier que le pan natif reste coordonné |
| 10 | Symétrie basse | **Non résolu**, tracer le mode et l'ancrage du curseur |
| 11 | Focus/resize/save state | Tester retour de focus, resize, DPI 100/150/200 %, chargement avec pointeur immobile puis mouvement |
| 12 | Régressions | Comparer rendu, menus, traduction FR, Sylt, son, SRAM/save states et ratios 4:3/16:9/21:9/32:9 |

Pour le pointage F3 natif, vérifier aussi la sélection, le glissement continu
route/bulldozer, le pan droit, le retour à la souris après le clavier/stick et
l'absence de mouvement dans les pages advisor. Les déplacements/recentrages
imposés par le jeu peuvent toujours contredire le pointeur ; ils nécessitent
la future adaptation des interactions, pas une simple conversion SDL.

## Compiler Windows Release

Depuis PowerShell avec CMake, Ninja, MinGW-w64 x64 et SDL3 installés
(par exemple MSYS2 `mingw64`) :

```powershell
git clone https://github.com/davidbressy-oc/UrbanRecomp.git
cd UrbanRecomp
git switch fix/native-gamepad-widescreen-input
git submodule update --init --recursive
$env:Path = "C:\msys64\mingw64\bin;" + $env:Path
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DSNESRECOMP_SDL_BACKEND=SDL3 -DSC_AOT=OFF -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

`SC_AOT=OFF` évite les banques générées à partir de la ROM ; aucune génération
ni modification de ROM n'est nécessaire. Pour SDL2, utiliser un autre dossier
et `-DSNESRECOMP_SDL_BACKEND=SDL2`, avec le SDK SDL2 correspondant.
Le script existant `tools/run_custom_renderer.ps1 -BuildOnly` constitue aussi
une entrée de compilation ; ne pas régénérer les banques ROM pour distribuer.

## Déployer sans perdre les données

1. Fermer le jeu. Copier le dossier d'installation complet en sauvegarde.
2. Tester d'abord dans une copie de l'installation, surtout pour F3 : la
   surface 32:9 n'est pas encore corrigée. Garder l'exécutable précédent pour
   revenir en arrière.
3. Remplacer `UrbanRecomp.exe` et les DLL nécessaires provenant du **même build**
   (`SDL3.dll`, ainsi que les runtimes MinGW si la chaîne les lie dynamiquement).
   Garder les assets et traductions existants ; compléter les assets du build
   seulement s'ils manquent. Ne pas remplacer un DLL SDL3 par SDL2, ou inversement.
4. **Conserver** `sc-settings.ini`, `sc-video.ini`, `config.ini`, `keybinds.ini`,
   tous les fichiers de sauvegarde SRAM et tous les save states. Ne pas copier
   les fichiers de test `sc-test-*.ini` ni des paramètres provenant d'un autre
   dossier. Garder la même ROM personnelle et le même répertoire de travail.
5. Au premier lancement, sélectionner le périphérique et vérifier ses mappings,
   puis Play. Les anciens paramètres sont compatibles ; `[Input]` s'ajoute au
   prochain enregistrement du launcher. Les formats de sauvegarde restent ceux
   de la v1.1.1.
