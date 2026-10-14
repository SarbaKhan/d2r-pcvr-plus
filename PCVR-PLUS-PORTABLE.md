# D2R PCVR Plus — mise à jour portable Windows x64

Ce ZIP met à jour une **installation D2R PCVR existante créée par le setup automatique**.
Il contient la DLL compilée, les profils Flat/VR, les lanceurs, l'outil de mise à jour
et Python 3.12.10 x64 embarqué avec sa bibliothèque standard et sa licence.
Aucune installation de Python, aucun compilateur et aucun téléchargement au premier
lancement ne sont nécessaires. Conserver le dossier extrait entier.

## Installation de la mise à jour

1. Fermer D2R et ses lanceurs. Extraire `D2R-PCVR-Plus-Windows.zip` dans un dossier
   permanent accessible en écriture, distinct du dossier du jeu.
2. Exécuter `Configure.cmd`. Indiquer le dossier contenant `D2R.exe` et
   `D2RLoader.exe`. Aucun dossier de personnages ou de backup n’est demandé.
   Choisir Steam ou Battle.net, puis **Steam Link / SteamVR**,
   **Virtual Desktop / VDXR**, **Flat Screen** ou **All** (trois lanceurs CMD).
   Pour SteamVR ou All, confirmer le chemin réel de `steamxr_win64.json`.
   Les chemins sont mémorisés dans `portable-settings.json` à côté des lanceurs.
3. Exécuter `Update.cmd`. Il sauvegarde et vérifie l'ancienne DLL ainsi qu'une copie
   du profil VR dans `d2rloader/pcvr-plus-backups/<identifiant>/`, remplace la DLL
   et ajoute `d2r_pcvr_plus_flat.ini` seulement si ce fichier n'existe pas déjà.
4. Utiliser le lanceur `D2R-PCVR-PLUS-*.cmd` du mode souhaité. Le jeu utilise son
   emplacement de sauvegarde habituel, sans copie automatique ni redirection.

D2RLoader, ReShade, OpenXR, les ressources du setup, `pcvr-install.json`, le profil
VR actif `d2rloader/plugins/d2r_vr.ini`, les réglages Flat existants et les personnages
ne sont pas remplacés. `profiles/d2r_vr.reference.ini` est un **modèle de référence** :
ne pas le copier sur votre configuration VR. Le profil Flat actif se trouve dans
`d2rloader/plugins/d2r_pcvr_plus_flat.ini`; voir `PCVR-PLUS.md` pour les modes
isométrique / troisième personne / première personne et les touches ZQSD/WASD.

Le paquet réutilise les dépendances natives déjà installées par le setup. Il ne
réinstalle ni D2RLoader ni ReShade ni le runtime VR. Le plugin utilise les composants
système Windows et des bibliothèques C++ liées statiquement. Flat n'exige pas de
casque ni de runtime OpenXR actif. VR conserve ses prérequis matériels habituels.

Les versions du jeu actuellement acceptées sont celles de `game_install.py` :
Steam 3.3.93787, Battle.net 3.3.93847 (expérimental). Pour Battle.net, lancer depuis
un terminal avec `Launch-Flat-Plus.cmd --experimental-battlenet` (ou le lanceur VR).
Les contrôles de version restent actifs. Usage hors ligne uniquement.

## Retour arrière

Fermer le jeu, puis exécuter `Rollback.cmd`. Il restaure uniquement l'ancienne DLL,
après vérification des empreintes. Les profils et les personnages restent en place;
le nouveau profil Flat peut rester présent, inactif avec l'ancienne DLL. Les copies
de sécurité restent disponibles. Une seconde mise à jour est refusée tant que la
précédente n'a pas été annulée. Si un autre outil a modifié la DLL entre-temps,
le retour arrière refuse de l'écraser : examiner les copies manuellement.

Une interruption brutale peut laisser `d2rloader/pcvr-plus-update.lock`. Après avoir
vérifié que le jeu et l'outil sont fermés, retirer ce verrou et utiliser `Rollback.cmd`.
Le journal est écrit avant le remplacement de DLL. Ne pas supprimer le journal
`pcvr-plus-update.json` ni son dossier de sauvegarde avant le retour arrière.
Si le setup automatique est réutilisé ultérieurement, il peut remplacer cette DLL :
annuler d'abord la mise à jour Plus.

Sans configuration interactive :

```bat
Update.cmd --game "D:\Games\Diablo II Resurrected"
Rollback.cmd --game "D:\Games\Diablo II Resurrected"
Launch-Flat-Plus.cmd --game "D:\Games\Diablo II Resurrected"
```

## Validation et limites

GitHub Actions compile sous Windows, exécute les tests C++ et Python, crée le ZIP,
puis l'extrait dans un dossier temporaire. Le test du paquet utilise son propre
Python, un PATH limité aux outils système et une fausse installation. Il vérifie
les wrappers CMD, la mise à jour, le retour arrière et la conservation des fichiers.
L'artifact `D2R-PCVR-Plus-Windows` n'est téléversé qu'après réussite de ces étapes.
Le fichier `.sha256` permet de contrôler le ZIP téléchargé.

**À tester sur le PC réel avec des personnages de test :** lancement Flat sans casque,
les trois caméras, souris dans les menus/inventaire, déplacement et attaques;
puis le lancement VR, la sélection du runtime, le suivi physique, les interactions
et l'absence de régression. La CI ne lance pas D2R et ne prouve pas la stabilité en jeu.
Voir le protocole détaillé dans `PCVR-PLUS.md`. Aucun fichier propriétaire de Blizzard
n'est fourni. Les licences et attributions se trouvent dans `LICENSE`, `THIRD_PARTY.md`,
`licenses/` et `deps/python/LICENSE.txt`.

## Clavier Flat et contrôleurs Quest

Le profil livré utilise désormais QWERTY (`flat_layout=WASD`). Une mise à jour
conserve le profil déjà installé : pour passer une installation AZERTY en QWERTY,
fermer le jeu et modifier uniquement `flat_layout=WASD` dans la section `[input]`
de `d2rloader/plugins/d2r_pcvr_plus_flat.ini`. Si des touches personnalisées
`flat_forward`, `flat_left`, `flat_back` ou `flat_right` sont présentes, elles
priment sur la disposition : les régler respectivement sur W, A, S et D.
Aucune nouvelle DLL ni réinstallation n'est nécessaire pour ce changement.

Les contrôleurs VR sont associés automatiquement au profil actif communiqué par
OpenXR. Le code suggère déjà le profil Oculus Touch utilisé par le Quest 2 :
A/B à droite, X/Y à gauche, avec un clic de stick distinct pour chaque main.
Le réglage en jeu « Headset / controller labels » → « Quest / Touch » corrige
uniquement les libellés; il ne remappe pas les entrées et ne répare pas un bouton
non détecté. Ne pas remplacer le profil VR existant par le modèle du paquet.

Pour diagnostiquer des commandes absentes, préciser la liaison (Link, Air Link,
Virtual Desktop ou Steam Link), le runtime OpenXR réellement utilisé et si les
boutons fonctionnent dans les menus ou seulement pas en jeu. Joindre
`d2rloader/logs/d2r-vr-frame.log` après une session concernée et préciser les
actions obtenues et attendues pour chaque clic de stick. Les fichiers de gestion
des profils et commandes VR restent identiques à la base PCVR retenue.

## Sélection explicite du runtime VR

Les raccourcis du setup passent par `steam.exe -applaunch 2536520 --frame-vr`
(avec `--frame-vdxr` pour Virtual Desktop), puis par les options du jeu Steam.
Ces options sélectionnent explicitement le manifeste SteamVR pour le premier
raccourci. Le lanceur Plus appelle directement la même fonction de lancement
Windows, mais ne lit pas les options Steam du jeu.

Le configurateur propose désormais quatre choix :

- `steamvr` : transmet le manifeste choisi à `--frame-openxr-runtime=...`
  dans le lanceur d'origine, qui définit `XR_RUNTIME_JSON` pour ce lancement;
- `vdxr` : transmet `--frame-vdxr`, comme le raccourci Virtual Desktop;
- `flat` : écran classique sans runtime VR;
- `all` : crée les trois lanceurs indépendants ci-dessous.

Les anciennes configurations avec `vdxr=true` restent compatibles. Celles avec
`vdxr=false` ne distinguaient pas SteamVR du runtime système : relancer
`Configure.cmd` pour choisir explicitement. Aucun réglage OpenXR global ni
raccourci Steam n'est modifié. Le runtime sélectionné est affiché au lancement.

Pour cette correction, la DLL est inchangée : remplacer les deux scripts
`steamvr/portable_plus.py` et `steamvr/launch_plus.py` du paquet extrait par
ceux du nouveau ZIP, puis relancer `Configure.cmd`. Ne pas relancer `Update.cmd`
si la DLL Plus est déjà installée. On peut aussi extraire le nouveau paquet dans
un autre dossier et le configurer pour la même installation.

Surcharge ponctuelle (nouveau lanceur uniquement) :

```bat
Launch-VR-Plus.cmd --openxr-runtime "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json"
Launch-VR-Plus.cmd --vdxr
Launch-VR-Plus.cmd --system-openxr
```

Test matériel : comparer le raccourci SteamVR du setup au lanceur Plus configuré
avec exactement le même manifeste, puis vérifier A/B/X/Y et les clics des sticks
dans les menus et en jeu. La sélection explicite corrige une différence réelle
de lancement; la résolution du problème de commandes Quest reste à confirmer.

## Suppression des backups de personnages

Le lanceur n’exige plus `Settings.json`, ne parcourt plus les sauvegardes et ne
crée plus de copie ni de verrou à côté de leur dossier. Les options `--saves` et
`--backups` sont supprimées. Dans un ancien `portable-settings.json`, ces deux
champs sont ignorés; relancer `Configure.cmd` produit une configuration sans eux.
Aucune migration de personnages n’est nécessaire : le lanceur précédent ne
changeait déjà pas leur emplacement, il en créait seulement des copies. Les
anciennes copies restent sur le disque, sans être supprimées automatiquement.

Pour mettre à jour une installation Plus existante, remplacer seulement les
fichiers `steamvr/launch_plus.py` et `steamvr/portable_plus.py` dans le dossier
portable puis relancer `Configure.cmd`. Aucun changement de DLL n’est nécessaire.
La copie de l’ancienne DLL et du profil VR effectuée par `Update.cmd` reste
disponible pour le retour arrière; elle ne concerne pas les personnages.

## Trois lanceurs indépendants pour les tests

Exécuter `Configure.cmd` et choisir **All** (ou 4). Le configurateur crée,
dans le dossier portable :

| Fichier | Mode imposé |
|---|---|
| `D2R-PCVR-PLUS-SteamVR.cmd` | VR avec le manifeste SteamVR configuré |
| `D2R-PCVR-PLUS-VDXR.cmd` | VR avec `--frame-vdxr` |
| `D2R-PCVR-PLUS-Flat.cmd` | Flat, sans sélection de runtime VR |

Les trois coexistent; aucun changement de configuration n'est nécessaire pour
passer d'un mode à l'autre. Fermer le jeu avant de lancer un autre mode. Choisir
un seul mode génère uniquement son lanceur; un ancien lanceur généré est retiré
si désélectionné, mais un fichier CMD modifié manuellement n'est jamais écrasé.
Le choix Flat seul n'exige aucun manifeste SteamVR. Les chemins déjà enregistrés
sont proposés lors d'une nouvelle configuration. Pour Battle.net, les contrôles
expérimentaux restent actifs : ajouter `--experimental-battlenet` au lanceur.

Les anciens `Launch-Flat-Plus.cmd` et `Launch-VR-Plus.cmd` sont conservés pour
compatibilité. Avec All, le lanceur VR générique utilise SteamVR; employer les
noms explicites ci-dessus pour choisir sans ambiguïté. Le runtime système reste
accessible via l'option avancée `Launch-VR-Plus.cmd --system-openxr`.

Pour cette mise à jour, remplacer `steamvr/portable_plus.py` puis relancer
`Configure.cmd`; la DLL ne change pas. Aucun raccourci Steam n'est créé ou modifié.
L'intégration des quatre choix dans le setup graphique et la création des trois
entrées de bibliothèque Steam sont réservées à la version finale. Cette étape
de tests reste une mise à jour d'une installation PCVR existante, sans nouveau
setup complet ni demande de dossier de personnages.
