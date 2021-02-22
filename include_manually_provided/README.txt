This folder is headers the releaser must gather themselves and
include here (usually from the output of building the libraries in question for
lib_manually_built). They are not checked in.

Git will ignore any header files placed in here.

Mostly for shared libraries that you install via your package manager when
building from source, but in binary distribution we do want to ship them.
