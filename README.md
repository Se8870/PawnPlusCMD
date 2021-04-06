# pawn-plus-commands

[![sampctl](https://img.shields.io/badge/sampctl-pawn--plus--commands-2f2f2f.svg?style=for-the-badge)](https://github.com/AGraber/PawnPlusCMD)

<!--
Short description of your library, why it's useful, some examples, pictures or
videos. Link to your forum release thread too.

Remember: You can use "forumfmt" to convert this readme to forum BBCode!

What the sections below should be used for:

`## Installation`: Leave this section un-edited unless you have some specific
additional installation procedure.

`## Testing`: Whether your library is tested with a simple `main()` and `print`,
unit-tested, or demonstrated via prompting the player to connect, you should
include some basic information for users to try out your code in some way.

And finally, maintaining your version number`:

* Follow [Semantic Versioning](https://semver.org/)
* When you release a new version, update `VERSION` and `git tag` it
* Versioning is important for sampctl to use the version control features

Happy Pawning!
-->

## Installation

Simply install to your project:

```bash
sampctl package install AGraber/pawn-plus-commands
```

Include in your code and begin using the library:

```pawn
#include <PawnPlusCMD>
```

## Usage

```pawn
CMD:test(playerid, number, string[], floatNumber)
{
    printf("playerid called test command with number %d, string %s, and float number %f", number, string, floatNumber);
    SendClientMessage(playerid, -1, "hey");
    return 1;
}
```
