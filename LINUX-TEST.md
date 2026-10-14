# D2R PCVR Plus — validation Linux, étape 1

Paquet EXPÉRIMENTAL : le lancement Linux réel reste à valider. Pas encore
l'assistant Qt. Cible initiale : CachyOS x86-64, Steam natif, édition Steam de D2R
3.3.93787. Steam Flatpak et Battle.net ne sont pas pris en charge par ce kit.
Aucun fichier propriétaire du jeu n'est distribué. Aucun personnage n'est copié,
déplacé ou restauré. Le préfixe Proton et les sauvegardes restent ceux du jeu.

## Préparer

- Steam, le jeu acheté et Python 3.10+ doivent être disponibles. Ce kit technique
  utilise le Python Linux ; il ne contient pas Python Windows. Aucun compilateur
  n'est requis. Le futur assistant autonome sera un travail séparé.
- Lancer D2R normalement sous Proton, vérifier le fonctionnement et quitter.
- Extraire le TAR.GZ dans un dossier permanent, ouvrir un terminal dans ce dossier.
- Pour cette première validation, garder le Proton choisi dans les propriétés du
  jeu et noter sa version exacte. Si le pont OpenXR échoue avec ce Proton, comparer
  avec Proton Experimental, sans supprimer le préfixe.

Configurer les chemins (guillemets nécessaires pour les espaces) :

```sh
python3 steamvr/linux_test.py configure --game "/chemin/bibliothèque/steamapps/common/Diablo II Resurrected" --steam "/usr/bin/steam"
python3 steamvr/linux_test.py install
```

`--steam` désigne le lanceur Linux Steam, pas son dossier de données ni steam.exe.
Le dossier du jeu peut appartenir à une autre bibliothèque/disque. Pour un préfixe
personnalisé, ajouter `--saves "/chemin/vers/Saved Games/Diablo II Resurrected"` à
`configure` : cela localise Settings.json, sans déplacer les sauvegardes.
L'installation télécharge D2RLoader, ReShade et le compilateur de shaders Microsoft
avec contrôle des empreintes. Elle sauvegarde les fichiers du mod remplacés et
Settings.json pour permettre leur restauration, jamais les personnages.

Copier la ligne affichée par `configure` dans les options de lancement de D2R
sur Steam. Conserver les éventuels paramètres nécessaires à WiVRn devant cette
ligne, avec un seul `%command%`. Conserver une copie des anciennes options.
Aucun raccourci Steam ni réglage global n'est modifié automatiquement à cette étape.

## 1. Flat, sans casque ni serveur VR

```sh
python3 steamvr/linux_test.py flat
```

Vérifier les trois caméras, WASD, souris, clic gauche/droit, inventaire, menus,
dialogues PNJ, chargement d'un personnage temporaire et sortie normale. Vérifier
que Steam repasse à « Jouer », puis relancer. Tester aussi le lancement original
via le bouton Jouer de l'entrée achetée : il doit rester non moddé.

## 2. SteamVR

Relancer `configure` avec les mêmes chemins et ajouter :

```text
--steamvr-runtime "/chemin/bibliothèque/steamapps/common/SteamVR/steamxr_linux64.json"
```

Choisir le JSON Linux réellement présent sur la machine (pas steamxr_win64.json).
Connecter le casque via le parcours SteamVR habituel et vérifier un autre jeu VR.
Puis :

```sh
python3 steamvr/linux_test.py steamvr --probe
python3 steamvr/linux_test.py steamvr
```

Le probe remplace D2R par un petit programme diagnostic dans la même commande
Proton. Il crée puis ferme une session OpenXR/D3D12 ; il ne lance pas D2R.
Ne lancer le jeu qu'après avoir vérifié le résultat du probe.

## 3. WiVRn

Connecter le casque au serveur WiVRn. Relancer `configure` avec les mêmes chemins
et `--wivrn-runtime "/chemin/vers/openxr_wivrn.json"`.
Le chemin et les paramètres Steam requis dépendent du paquet WiVRn : suivre ceux
affichés par son dashboard. Le fichier actif peut être un lien symbolique : il est
résolu à la configuration. Une mise à jour changeant sa cible peut nécessiter une
nouvelle configuration. Le kit ne change jamais le runtime global.
Si WiVRn est un Flatpak, ses bibliothèques doivent aussi être accessibles au
conteneur Steam Linux Runtime ; conserver les paramètres du dashboard.

```sh
python3 steamvr/linux_test.py wivrn --probe
python3 steamvr/linux_test.py wivrn
```

SteamVR ne doit pas être nécessaire sur cette route. Une erreur D3D12/OpenXR du
probe signifie que le pont Proton/runtime est à examiner ; ne pas désactiver les
contrôles d'extensions. ALVR utilise la route SteamVR ; pas de validation ALVR
revendiquée.

## Résultats à relever

Pour chaque route : Proton exact, session KDE Wayland/X11, GPU/pilote, runtime et
version, résultat du probe, démarrage, commandes A/B/X/Y et clics sticks, position
des panneaux, dialogue PNJ visible, déplacement/combat, sortie puis relance.
Tester 15–30 minutes, un changement de zone et un retour au menu. En Flat, vérifier
le retour normal du curseur dans les fenêtres. Si capture souris/focus défaillants
sous Wayland, noter si Proton utilise Wayland natif ou XWayland avant de comparer.

Journaux dans le dossier du jeu :

- `d2rloader/logs/plus-linux-launch.jsonl` : mode, runtime, préfixe et code de sortie.
- `d2rloader/logs/plus-steamvr-probe.txt` ou `plus-wivrn-probe.txt`.
- `d2rloader/logs/steam-2536520.log` : Proton.
- Journaux du mod et `ReShade.log`.

Erreur avant lancement : `linux-launch-error.txt` dans le dossier du kit ou sortie
terminal. Le code de retour du petit lanceur Steam signifie seulement que la
requête a été transmise ; le résultat réel est dans les journaux ci-dessus.
Les logs peuvent contenir des chemins personnels : les relire avant de partager.

Pour restaurer les fichiers remplacés, utiliser la sauvegarde d'installation
indiquée par install.py :

```sh
python3 steamvr/install.py --game "/chemin/du/jeu" --restore "/chemin/du/kit/installation-backups/SAUVEGARDE"
```

Quitter D2R au préalable. Remettre les anciennes options Steam après restauration.
Ne pas supprimer le dossier du kit tant que Steam pointe vers son lanceur.
