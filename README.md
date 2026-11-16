<p align="center">
    <img src="https://content.globalconflicts.net/ArtAssets/logo_black_text.png" width="1024">
</p>
<p align="center">
    <a href="https://github.com/Global-Conflicts-ArmA/gc-reforger-missions/releases/latest">
        <img src="https://img.shields.io/badge/Version-1.0.2-blue.svg" alt="Global Conflicts Reforger mission repo version">
</p>

# gc-reforger-missions

[GC Missions](https://reforger.armaplatform.com/workshop/62D55563549DA97A-GlobalConflictsMissions) is a repository of Arma Reforger missions created by and for the Global Conflicts community.

## Contributing

Information on how to contribute can be found in the [GC Missions Wiki](https://github.com/Global-Conflicts-ArmA/gc-reforger-missions/wiki).

## Dependency ownership

Missions owns mission worlds, headers, terrain-specific navigation/map data and
mission-specific scripts/configuration. Vehicle, weapon and unit overrides belong
to GC Units; shared systems belong to GC Core. The relocated Core mission data
retains its GUIDs. Update all three Dev checkouts together.

This project declares Core, Units and Lobby Dev, its terrain library, and the
TilW framework still used by legacy mission data. The relocated legacy TilW
mission-framework/controller presets remain compatibility resources; they are
not the supported GC Lobby mode. GC Lobby and Core do not depend on TilW.
GC Workshop is the full collection and is never a dependency of Missions.
