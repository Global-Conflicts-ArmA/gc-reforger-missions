# Broken migration drafts

These ten converted drafts reached GC Lobby pregame in dedicated-server smoke tests. Missing resources still affect roles, vehicles, or objectives. The drafts remain outside the playable mission catalogue.

Every resource and metadata filename ends in `.disabled` so Workbench sees no launchers to register here. The bytes and relative paths from the migration audit are preserved. The [manifest](manifest.json) lists each launcher, world, file hash, and unresolved reference.

| Mission | Missing resource references |
| --- | ---: |
| Rocky Road | 4 |
| Al Hadra Rumble | 2 |
| Cobra's Bite | 3 |
| Abad Day in Kunar US TVT | 3 |
| Nagalam Traffic Jam | 21 |
| Gull Bay Guns | 1 |
| Factory Siege Redux | 1 |
| Heaven and Hell | 1 |
| Hot Boxes | 2 |
| Syrian Checkpoint | 2 |

To resume a port, extract the files listed for that mission to an external staging directory and remove the `.disabled` suffix there. Verify their hashes against the manifest, restore the missing dependencies, and rerun the migration audit, smoke tests, and native metadata capture. The manifest identifies the Dev base for shared resources.
