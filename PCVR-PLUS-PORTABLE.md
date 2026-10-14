# D2R PCVR Plus — mise à jour portable Windows x64

Ce ZIP met à jour une **installation D2R PCVR existante créée par le setup automatique**.
Il contient la DLL compilée, les profils Flat/VR, deux lanceurs, l'outil de mise à jour
et Python 3.12.10 x64 embarqué avec sa bibliothèque standard et sa licence.
Aucune installation de Python, aucun compilateur et aucun téléchargement au premier
lancement ne sont nécessaires. Conserver le dossier extrait entier.

## Installation de la mise à jour

1. Fermer D2R et ses lanceurs. Extraire `D2R-PCVR-Plus-Windows.zip` dans un dossier
   permanent accessible en écriture, distinct du dossier du jeu et des sauvegardes.
2. Exécuter `Configure.cmd`. Indiquer le dossier contenant `D2R.exe` et
   `D2RLoader.exe`, le dossier de sauvegardes réellement utilisé (contenant
   `Settings.json`), puis un dossier permanent de sauvegardes de sécurité **séparé**.
   Choisir Steam ou Battle.net et le runtime VR habituel (VDXR ou non).
   Les chemins sont mémorisés dans `portable-settings.json` à côté des lanceurs.
3. Exécuter `Update.cmd`. Il sauvegarde et vérifie l'ancienne DLL ainsi qu'une copie
   du profil VR dans `d2rloader/pcvr-plus-backups/<identifiant>/`, remplace la DLL
   et ajoute `d2r_pcvr_plus_flat.ini` seulement si ce fichier n'existe pas déjà.
4. Utiliser `Launch-Flat-Plus.cmd` ou `Launch-VR-Plus.cmd`. Une copie vérifiée des
   sauvegardes est créée avant chaque lancement. Un échec de sauvegarde bloque
   le lancement. Les anciens raccourcis restent disponibles mais contournent
   cette protection. Ne pas lancer le jeu simultanément par un autre raccourci.

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
Launch-Flat-Plus.cmd --game "D:\Games\Diablo II Resurrected" --saves "D:\Saves\D2R" --backups "D:\Backups\D2R"
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
