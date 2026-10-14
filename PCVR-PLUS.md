# D2R PCVR Plus — première candidate Flat/VR

**État : compilation Windows vérifiée ; jouabilité et absence de régression VR à valider sur matériel réel.**
Cette branche reste en validation. L'utilisateur a identifié les freezes et corruptions signalés comme un problème de son PC, désormais résolu, et non de D2R VR. À sa demande, les lanceurs n'effectuent plus de backups de personnages et laissent le jeu gérer ses sauvegardes natives.

La base reste [bearstonem/d2r-pcvr](https://github.com/bearstonem/d2r-pcvr), commit `6abf4e52c13fae5eea11aa5defd7618645d706e9`.
Le dépôt de référence [natpoh/diablo2-vr-port](https://github.com/natpoh/diablo2-vr-port) a été examiné au commit `4b6284c0766c089139693a504333b9a19201cdd2`.
Aucun nouveau code de cette référence n'a été transplanté : PCVR possède déjà les caméras Flat et la plupart des commandes nécessaires.

## Analyse technique et choix

| Domaine | D2R VR de référence | Base PCVR et décision Plus |
|---|---|---|
| Caméras | `vr/vrcam.cpp` : `ViewNow`, `ApplyView`, `VrViewInner`, `VrProjInner`, F1/F2/F3 Flat | Fonctions déjà présentes. Conserver `cleanroom/camera` de PCVR et ses corrections ; réactiver leur parcours mono. |
| VR | Plusieurs transports et intégrations BodyWalk/FlatVR, OpenXR propre à cette branche | Conserver `steamvr/transport.cpp`, les poses OpenXR et le rendu stéréo de PCVR. Ne pas les remplacer. |
| Marche | `FlatMoveInput`, `HookKeyMove` : clavier tourné selon la caméra, pipeline natif du jeu | Réutiliser le code PCVR. Ajouter ZQSD/WASD et remappage A–Z. Pas de nouveau moteur de déplacement. |
| Souris | `GameWndProc`, `MouseTick`, `HookMapClick` : recentrage et clic natif ; F9 libère le pointeur | Conserver le chemin souris. Libérer le pointeur en menus/chat et empêcher la capture VR de prendre les événements Flat. |
| Commandes | `gamecmd.cpp` injecte les commandes du pont sur le thread UI | Garder les modifications PCVR (notamment potions VR). Ne pas lire les commandes/actions/poses BodyWalk en Flat. |
| Hooks | Signatures et adresses dépendantes du build du jeu | Conserver les gardes PCVR. Pas de remplacement d'adresses à l'aveugle. Le repli Flat vers une manette virtuelle est supprimé. |
| Démarrage | Sélection de plateforme dans l'INI | Le transport PCVR s'enregistrait sans condition et pouvait tenter OpenXR même en Flat. Plus empêche cet enregistrement en Flat. |
| Sauvegardes | Données du jeu, hors caméras | Aucune écriture de personnages ajoutée au plugin. Lanceur avec copies complètes vérifiées avant chaque session. Pas de redirection non documentée. |

Le CMake à utiliser est **`steamvr/CMakeLists.txt`**. Celui de la racine construit aussi des éléments historiques ; ce n'est pas la cible PCVR actuelle.
La référence comporte des changements importants supplémentaires (notamment ses propres intégrations OpenXR et modules `cleanroom/heft`) : ils ne sont pas importés.

Plan appliqué : sélectionner la plateforme avant les hooks, figer cette sélection jusqu'au redémarrage, séparer les profils, isoler les entrées Flat, compiler et tester. La validation visuelle et fonctionnelle est la dernière étape encore ouverte.

## Ce que contient la candidate

- Une seule DLL `d2rl-vrcam.dll` pour Flat et VR.
- F1 : caméra isométrique et clics classiques du jeu. F2 : troisième personne avec souris. F3 : première personne avec souris. F12 parcourt ces trois vues en Flat.
- F2/F3 : marche WASD par défaut ; ZQSD ou quatre lettres A–Z configurables, diagonales normalisées dans le code existant. F1 conserve le déplacement classique par clic.
- Clic gauche : attaque sur ennemi, interaction sur objet/PNJ/porte ; clic dans le vide traité comme attaque sur place si le hook de clic est compatible. Clic droit : compétence secondaire native. Affecter les compétences dans le jeu.
- F9 : libère/reprend le pointeur. Menus, inventaire et chat gardent les entrées natives. Le changement F1/F2/F3 est prévu en jeu, pas pendant la saisie d'un texte.
- Flat : pas d'enregistrement du transport OpenXR, pas de pont BodyWalk, pas de paire stéréo ni de hooks d'interface/combat physique VR. La DLL n'importe pas statiquement de chargeur OpenXR.
- VR : profil existant, transport, contrôles physiques et lanceur Windows d'origine conservés. Cela doit encore être confirmé au casque.
- Deux profils indépendants : `d2r_pcvr_plus_flat.ini` et `d2r_vr.ini`. Dans Flat, `[camera]` règle la première personne et `[third]` la distance/hauteur de troisième personne ; F1 utilise la caméra du jeu. Le FOV et les touches sont communs aux vues Flat.

Limites connues : pas de test dans D2R ici ; validation matérielle encore nécessaire pour exclure des régressions. Les hooks Flat peuvent ne pas correspondre à un autre build ; consulter les logs et revenir à F1/F9 si nécessaire. Le clic dans le vide peut redevenir une marche si le hook de clic échoue. Aucun multijoueur TCP/IP ni travail Proton ajouté.

## Compilation Windows

Depuis un clone de **cette branche**, avec Git et PowerShell :

```powershell
git clone --branch feature/flat-plus https://github.com/SarbaKhan/d2r-pcvr-plus.git
cd d2r-pcvr-plus
powershell -NoProfile -ExecutionPolicy Bypass -File steamvr/setup_windows_tools.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File steamvr/build_windows.ps1
.\deps\python\python.exe -m unittest discover -s steamvr -p launch_plus_test.py
.\deps\python\python.exe -m unittest discover -s steamvr -p launch_windows_test.py
.\deps\cmake-4.4.4-windows-x86_64\bin\ctest.exe --test-dir build-frame/windows-native --output-on-failure -R '^(flat-controls|math|gameplay|combat-rules|controller-bindings|spell-cast|bow-flight)-test$'
```

Sortie : `build-frame/windows-native/d2rl-vrcam.dll`. Les outils et SDK restent ceux épinglés par PCVR : LLVM-MinGW 20260616, PluginSDK `717f727a0ec52912d1558764345f8fa3453a2bd6`, OpenXR-SDK `57af7fc61f9f2d492580cb28aab6d0ea59d8d417`. Le SDK OpenXR sert à compiler la partie VR ; aucun runtime actif n'est nécessaire pour lancer Flat.

La CI `PCVR Plus Windows portable` compile sur Windows et exécute le sous-ensemble CPU/launchers. Les tests GPU, interface et casque restent séparés ; un résultat CI vert ne valide pas la jouabilité.

## Installation dans une installation PCVR existante

1. Quitter D2R/D2RLoader. Pour cette candidate, utiliser un personnage de test hors ligne.
2. Conserver le dossier source Plus : il contient les lanceurs et leurs modules Python. Exécuter la préparation des outils ci-dessus si `deps/python/python.exe` n'existe pas.
3. Garder l'installation PCVR fonctionnelle et ses fichiers ReShade/D2RLoader. Une installation neuve suit d'abord [INSTALL-WINDOWS.md](INSTALL-WINDOWS.md). Ne pas réutiliser les fichiers de D2R VR dans cette installation.
4. Copier l'ancienne `d2rloader/plugins/d2rl-vrcam.dll` dans un dossier de retour arrière **hors du dossier plugins**. Ne pas y laisser deux DLL du plugin.
5. Remplacer cette DLL par celle compilée ici. Copier `d2r_pcvr_plus_flat.ini` à côté. Ne pas remplacer `d2r_vr.ini` : il conserve la configuration VR actuelle.
6. Employer les lanceurs Plus ci-dessous. Flat et VR utilisent l'emplacement de sauvegarde normal du jeu, sans copie automatique ni redirection.

Le lanceur vérifie les versions que PCVR annonce : Steam `3.3.93787`, Battle.net `3.3.93847` en aperçu expérimental. Ce contrôle ne garantit pas à lui seul que chaque hook Flat fonctionne avec Battle.net.

Depuis le dossier source, adapter le chemin du jeu (les exemples ne sont pas des chemins détectés) :

```powershell
.\Launch-Flat-Plus.cmd --game "D:\Games\Diablo II Resurrected"
.\Launch-VR-Plus.cmd --game "D:\Games\Diablo II Resurrected"
```

Steam doit être disponible pour l'édition Steam. Lancer Flat directement depuis Windows, sans passer par un raccourci « lancement VR » de Steam. Aucun lancement de SteamVR/BodyWalk n'est demandé par le lanceur Flat. Pour VDXR, ajouter `--vdxr` au lancement VR uniquement. Pour Battle.net, ajouter `--store battlenet --experimental-battlenet` ; tests hors ligne seulement.

Le lancement Flat passe `D2R_PCVR_PLUS_MODE=flat` uniquement au processus enfant. Sans cette variable, le lancement PCVR historique utilise son INI habituel. Changer de Flat à VR demande de fermer puis relancer le jeu ; F1–F3 ne changent que la vue Flat.

Dans `d2rloader/plugins/d2r_pcvr_plus_flat.ini` :

```ini
[input]
flat_layout=WASD
; Pour QWERTY : flat_layout=WASD
; Facultatif : quatre lettres distinctes, insensibles à la casse.
; flat_forward=Z
; flat_left=Q
; flat_back=S
; flat_right=D
```

Les doublons conservent les dernières touches valides ; une valeur autre qu'une lettre reprend la valeur du layout. Éviter les conflits avec les raccourcis du jeu. Les réglages sont relus en cours de session. Ne pas utiliser l'éditeur VR d'origine pour le profil Flat : éditer son INI.

## Sauvegardes natives et retour arrière

Le jeu gère ses personnages et ses coffres dans son emplacement habituel. Le
lanceur Plus ne lit pas ce dossier, ne le redirige pas et ne crée aucun snapshot
ni verrou auprès des sauvegardes. Aucun chemin de sauvegarde n'est demandé.
Les anciennes entrées `saves` et `backups` du configurateur portable sont ignorées;
relancer `Configure.cmd` les retire du fichier de configuration. Les anciennes
copies déjà créées restent sur le disque, sans être utilisées ni supprimées.
Le contrôle de présence d'un processus D2R/D2RLoader reste actif au lancement.

Retour à PCVR : jeu fermé, remettre la DLL conservée et utiliser le lanceur VR historique. Le profil VR n'a pas été remplacé. Le jeu desktop sans mod reste accessible via la procédure PCVR habituelle.

## Résultats automatisés de cette livraison

Sur l'environnement Linux de développement, compilation **croisée Windows x64 Release** avec LLVM-MinGW 20260616 : toutes les cibles de `steamvr/CMakeLists.txt` ont été compilées/liées. Les avertissements existants de `vrcam.cpp` ne sont pas corrigés dans cette modification ciblée.

| Vérification | Résultat |
|---|---|
| DLL Windows et exécutables de fixtures | Compilation et édition de liens réussies |
| Table des imports DLL | Pas d'import OpenXR/BodyWalk/SteamVR ; uniquement bibliothèques système/CRT |
| `launch_plus_test.py` | 9 tests : profils, routage Flat/VR, runtime explicite et lancement sans dossier de sauvegardes |
| `launch_windows_test.py` | 8 réussis, 3 ignorés car Windows requis |
| `flat_controls_test.cpp` compilé et exécuté sur hôte | Réussi |
| Six fixtures PCVR portables compilées et exécutées sur hôte | `math`, `gameplay`, `combat-rules`, `controller-bindings`, `spell-cast`, `bow-flight` : réussies |
| Exécution des `.exe` Windows, D3D12, chargement dans D2R, casque | Non exécutés dans cet environnement ; suivre la CI et les essais ci-dessous |

## Protocole matériel obligatoire avant validation

Noter : hash de DLL, version du jeu, édition, GPU/pilote, classe, acte, résolution, runtime VR et casque. Ne partager ni sauvegardes ni dumps propriétaires dans le dépôt.

| Étape | Scénario et résultat attendu |
|---|---|
| Sans runtime | Casque débranché, SteamVR/BodyWalk fermés, aucun runtime OpenXR actif : Flat atteint le menu puis un personnage de test. Log « Flat session; OpenXR transport disabled », sans tentative d'initialisation XR. |
| F1 | Clic pour marcher/attaquer, menus/inventaire/coffre/marchand ; caméra classique, aucune capture du pointeur. |
| F2/F3 | Rotation complète, regard haut/bas, ZQSD puis WASD et touches personnalisées ; marche relative à la caméra, diagonales sans accélération, arrêt au relâchement. Tester virages et collisions. |
| Clics | Mêlée, arc et sorts : clic gauche sur ennemi et dans le vide ; clic droit compétence secondaire ; maintien/relâchement. Aucun déplacement inattendu quand le hook attaque est disponible. |
| Interactions | Butin, portes, PNJ, waypoint et portail ; menus, inventaire, glisser-déposer, coffre partagé, marchand. Curseur libre, clics ne déclenchant pas d'action monde involontaire. |
| Interruptions | Alt-Tab en marchant/attaquant, retour, chat Enter/Esc, F9, chargement/mort/changement d'acte ; aucun déplacement ou bouton bloqué. |
| Persistance | Fermer normalement, recharger personnage/coffres depuis leur emplacement habituel ; vérifier que `d2r_vr.ini` est inchangé après Flat. |
| Retour VR | Relancer avec casque ; stéréo, suivi tête/mains, déplacement physique, arc, mêlée, bouclier, sorts, potions, menus/lasers, recentrage et calibration comme avant. |
| Endurance | Sessions Flat puis VR prolongées sur personnage de test, plusieurs actes/chargements/retours menu ; relever freezes, erreurs et stabilité des sauvegardes. |

Aucun de ces essais matériels n'a été réalisé par l'agent. La version ne sera qualifiée de fonctionnelle/validée qu'après cette étape. TCP/IP puis Linux/Proton feront l'objet de travaux distincts.

## Licences

Conserver [LICENSE](LICENSE), [THIRD_PARTY.md](THIRD_PARTY.md) et les notices des sources/SDK. D2R PCVR Plus désigne ce fork ; les crédits à D2R PCVR, D2R VR/BodyWalkVR et leurs dépendances sont conservés. Le nom n'implique aucune affiliation à Blizzard. Aucun exécutable, archive, personnage ou cache extrait de Blizzard n'est ajouté à cette livraison. Les dépendances de compilation ne sont pas commitées.
