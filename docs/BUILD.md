# Compilation et tests

## Environnement

Windows x64, Visual Studio ou Build Tools avec les outils C++ de bureau, un Windows SDK et PowerShell.
`Build.ps1` repère le toolset MSVC et le SDK installés avec `vswhere`.
Les dépendances de ce snapshot sont incluses dans `ext/`, `compat/` et `vendor/` : aucun sous-module à récupérer.

## Compiler

Depuis la racine du dépôt :

```powershell
.\Build.ps1
```

Sortie : `bin\x64\Prism.exe`. La version publiée vise x64. La configuration Win32 héritée n’a pas été validée pour Prism.
Ne change pas les réglages de protection du système pour faire passer une compilation. Si un composant est bloqué, conserve le diagnostic et considère la compilation comme bloquée.

## Tests isolés

```powershell
.\Build.ps1 -Tests
.\tools\Test-Render.ps1
```

La première commande compile le programme de tests et travaille dans `.build\test-data`.
Elle ne lie pas le moteur d’injection. Les tests de fermeture normale utilisent leurs propres fenêtres de test.
La seconde compile uniquement le renderer et les ressources graphiques, puis produit les captures et le rapport dans `.build\renders`.
Ces commandes ne lancent pas CS2, ne chargent pas de DLL dans un autre programme et n’utilisent pas le catalogue personnel.

Le workflow GitHub Actions exécute ces deux commandes. Il ne valide ni l’injection réelle ni le comportement des anti-triche.

## Organisation

| Chemin | Rôle |
|---|---|
| `src/Prisme.cpp`, `src/PrismeSurface.h` | Interface Win32 et rendu |
| `src/Catalogue.cpp`, `src/VersionStore.h` | Catalogue et conservation des versions |
| `src/CatalogueArchive.cpp` | Import et export ZIP |
| `src/Loader.cpp` | Coordination du parcours CS2 |
| `src/ProfileMgr.cpp` | Profil persistant |
| `tests/` | Tests sur données et fenêtres isolées |
| `preview/` | Aperçu et vérification du rendu |
| `ext/blackbone/` | Dépendance héritée, sources et ressources |

Les anciens noms Xenos / Prisme restent présents dans le projet et les chemins internes. Ne les remplace pas globalement : ils font partie du snapshot retrouvé.
Les sources C++ existantes sont conservées octet pour octet ; certaines chaînes utilisent des échappements Unicode pour la compilation MSVC.

## Version et fidélité

La version texte est `1.1.1.1.2.7`. Le champ numérique PE, limité à quatre composantes, vaut `1.1.2.7`.
Le tag conserve la version texte : `v1.1.1.1.2.7`.
La release contient le binaire historique, pas le nouveau binaire de contrôle. Voir [PROVENANCE.md](PROVENANCE.md).
