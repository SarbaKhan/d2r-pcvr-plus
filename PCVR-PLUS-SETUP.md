# D2R PCVR Plus — Setup 0.1

`D2R-PCVR-PLUS-Setup-0.1.exe` installe le mod complet sur une installation
Windows x64 de Diablo II: Resurrected, sans installation PCVR préalable.
Il contient le plugin Flat/VR, les profils, Python privé, le chargeur OpenXR
et CascLib. Il télécharge automatiquement D2RLoader et ReShade depuis leurs
sites officiels, avec vérification des empreintes approuvées : Internet est
nécessaire pendant l'installation. Aucun Python ni compilateur à installer.
Aucun fichier Blizzard n'est distribué ; les caches graphiques sont générés
localement à partir du jeu acheté et installé.

## Installation

1. Installer D2R via Steam ou Battle.net, lancer le jeu original une fois pour
   terminer les étapes de compte et créer `Settings.json`, puis le fermer.
2. Fermer complètement Steam et lancer le setup Plus.
3. Choisir l'édition, puis le casque utilisé pour les sessions VR. En Flat seul,
   ce choix n'active aucun matériel et peut rester sur sa valeur par défaut.
4. Choisir le mode :

   | Choix | Raccourcis ajoutés à Steam | Prérequis VR |
   |---|---|---|
   | Steam Link / SteamVR | SteamVR | SteamVR installé |
   | Virtual Desktop / VDXR | Virtual Desktop | Streamer et application casque |
   | Flat Screen (no VR) | Flat Screen | Aucun |
   | All (create three Steam library shortcuts) | Les trois | SteamVR et Virtual Desktop |

5. Vérifier les dossiers préremplis : Steam, jeu, Saved Games et, si nécessaire,
   SteamVR. Saved Games sert uniquement à lire/appliquer les réglages vidéo du
   jeu. Les personnages restent à leur emplacement natif, sans copie ni redirection.
6. Sélectionner le compte Steam local, puis **Full install** pour un jeu neuf.
   L'option **Update / repair** conserve les réglages existants. **Only configure
   Steam shortcuts** exige une installation Plus déjà complète.
7. Après installation, ouvrir Steam et utiliser les entrées SteamVR, Virtual
   Desktop ou Flat Screen dans la bibliothèque. Aucun CMD de lancement du jeu
   n'est créé. Le bouton Jouer du jeu original conserve le lancement sans mod.

Les fichiers auxiliaires restent dans `%LOCALAPPDATA%\Programs\D2R PCVR Plus`.
Ne pas déplacer ce dossier : les raccourcis y référencent Python et le lanceur.
Le setup ne change pas le runtime OpenXR global. Flat ne charge pas de runtime VR.
Il ne crée que les raccourcis sélectionnés ; une réparation met à jour les
entrées correspondantes sans doublons et conserve les autres raccourcis Steam.

## Modes et configuration

Le runtime VR reste celui validé sur Quest 2, issu de PCVR `918bd5e`.
Les profils du jeu sont `d2rloader/plugins/d2r_vr.ini` et
`d2rloader/plugins/d2r_pcvr_plus_flat.ini`. Flat utilise WASD par défaut,
F1/F2/F3 pour isométrique/troisième/première personne et F9 pour libérer la souris.
ZQSD et les touches personnalisées restent configurables dans le profil Flat.

Versions actuellement reconnues : Steam 3.3.93787 et Battle.net 3.3.93847
(expérimental). Les contrôles de version restent actifs. Utiliser des personnages
locaux hors ligne. Battle.net utilise lui aussi des raccourcis non-Steam dans Steam.

## Réparation et désinstallation

Le setup journalise les fichiers de mod remplacés, les réglages vidéo et les
modifications Steam pour permettre un retour arrière. Ces copies ne contiennent
pas les personnages. La désinstallation depuis les applications Windows restaure
les fichiers suivis et retire les raccourcis correspondants ; elle conserve les
personnages, la progression et les réglages vidéo actuels.

Pour une ancienne installation PCVR, conserver ses copies d'installation : le
setup peut en demander l'emplacement pour assurer une désinstallation complète.
Cette première version vise d'abord une installation propre. Un overlay Plus
portable antérieur doit être annulé avec son `Rollback.cmd` avant migration.

## Compilation et validation

Sur `feature/flat-plus`, avec Git et PowerShell :

```powershell
./steamvr/setup_windows_tools.ps1
./steamvr/build_windows.ps1
./installer/build_plus_release.ps1
```

Le dernier script compile les dépendances natives, vérifie et installe Inno Setup
7.1.0 dans les outils de compilation, construit l'EXE et contrôle son contenu.
Il exécute aussi le cycle Inno installation/désinstallation avec une application
factice isolée. La CI exécute en plus les tests du backend, du routage des trois
modes, de réparation et de retrait des raccourcis, ainsi que les tests Flat/VR
existants. L'EXE, son SHA-256 et son rapport de provenance sont publiés comme
artifact GitHub Actions. L'EXE n'est pas signé avec un certificat éditeur Plus.

À valider sur PC réel : installation depuis un D2R propre pour chaque édition,
apparition des trois entrées dans Steam, lancement de chacune et du jeu original,
Flat sans runtime VR actif, commandes/fenêtres/PNJ du Quest 2 sous SteamVR et
VDXR, réparation puis désinstallation. Les fixtures n'exécutent pas D2R et ne
remplacent pas ces essais matériels. Les licences et crédits upstream sont conservés.
