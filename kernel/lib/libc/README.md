# Do not use this

This only exists because gcc expects some functions of the libc to exist
and it likes to substitute code with calls to them sometimes.
These are quick stubs to make gcc happy.

You should not use these functions directly and instead prefer the alternative
implementations in the `lib` folder
