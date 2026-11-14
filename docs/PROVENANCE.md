# Provenance de la version

Vérifications effectuées le **14 novembre 2026**.

- Version texte : **1.1.1.1.2.7** ; version numérique PE : **1.1.2.7**.
- Le projet récupéré porte le nom historique **Xenos-Glass**.
- Son fichier `bin/x64/Prism.exe` et l’exemplaire original fourni ont le même SHA-256 et la même taille (**4,014,080 octets**).
- Empreinte du binaire livré : `b0f06ac179f440428d849e67731535b8b7fc32307aab241998419cffbbd6f950`.
- **386 fichiers du projet et de ses dépendances** ont été copiés sans modification. Seule la règle de fins de ligne Git de `ext/blackbone/.gitattributes` a été ajustée pour préserver leurs octets. La documentation publique, les captures et le workflow de vérification ont été ajoutés séparément.
- Le dossier de publication exclut les profils personnels, les archives de travail, l’historique Git privé, les rapports contenant des chemins personnels et les produits de compilation locaux.

## Ce que la correspondance établit

Le projet contient exactement l’exécutable recherché et les ressources de version concordantes. Une compilation complète en Release x64 depuis la copie exportée a réussi.
Il n’existe pas de manifeste historique permettant de prouver que chaque octet des sources actuelles a servi à produire l’exécutable original. Le nouveau binaire n’est donc pas présenté comme une reproduction certifiée de l’ancien.

SHA-256 de la compilation de contrôle : `cf0f9a38a72716177e75ff4284442e4d4a789c503ca46206a79c42c66c1bad0e`.
Identité intégrale avec l’original : **non**.

La release conserve le binaire original. Le rapport [verification.json](verification.json) précise les comparaisons et les tests exécutés.

## Validation et limites

Les tests isolés du catalogue, des profils, des versions, des ZIP et des diagnostics passent. Le test graphique exporte les PNG et contrôle les zones cliquables de 100 % à 200 %, sans échec signalé.
Ces tests ne lancent pas de partie et ne constituent pas une validation de l’injection, d’un redémarrage réel du jeu, des anti-triche ou d’un autre PC.
Les captures du README utilisent le moteur graphique du projet avec des données d’exemple.
