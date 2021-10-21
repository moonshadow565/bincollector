Tool made to collect .bin files from riot CDN mirrors.

```
Usage: bincollector [options] action manifest cdn

Positional arguments:
action                  action: list, extract, index, exever, checksum, [Required]
manifest                .releasemanifest / .manifest / .wad / folder [Required]
cdn                     cdn for manifest and releasemanifest (for raw wad files this is root of game folder)

Optional arguments:
-h --help               show this help message and exit
-o --output             Output directory for extract
-l --lang               Filter: language(none for international files).
-p --path               Filter: paths or path hashes.
-e --ext                Filter: extensions with . (dot)
--hashes-names          File: Hash list for names
--hashes-exts           File: Hash list for extensions
--skip-containers       Skip .wad processing
-w --show-wads          Show .wad files in dump
-d --max-depth          Max depth to recurse into.
```
