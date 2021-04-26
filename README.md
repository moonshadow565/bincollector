Tool made to collect .bin files from riot CDN mirrors.

```
Usage: bincollector [options] action manifest cdn

Positional arguments:
action          action: list, extract[Required]
manifest        .releasemanifest / .manifest / .wad / folder [Required]
cdn             cdn for manifest and releasemanifest

Optional arguments:
-h --help       show this help message and exit
-o --output     Output directory for extract
-l --lang       Filter: language(none for international files).
-e --ext        Filter: extensions with . (dot)
```
