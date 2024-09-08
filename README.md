# Dictu Mongo
A mongodb driver for [dictu](https://dictu-lang.com) build on Dictu's FFI api creating a wrapper around mongo's c driver.

## Building
**Note:** You need a version of dictu build from Dictus [develop branch](https://github.com/dictu-lang/Dictu/tree/develop) because the FFI API is not currently in a release.

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```
on mac/linux: `make`, on windows: `cmake --build . --config Release`.

Finally use `from "mod.du" import MongoDriver;` to import the module.

## Examples
See the `tests` folder.