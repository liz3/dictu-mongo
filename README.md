# Dictu Mongo
A mongodb driver for [dictu](https://dictu-lang.com) build on Dictu's FFI api creating a wrapper around mongo's c driver.

# Building
**Note:** You need a version of dictu build from Dictus [develop branch](https://github.com/dictu-lang/Dictu/tree/develop) because the FFI API is not currently in a release.

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```
on mac/linux: `make`, on windows: `cmake --build . --config Release`.

Finally use `from "mod.du" import MongoDriver;` to import the module.

# Docs
Here are exposed functions:
```
MongoDriver.createClient(url:string) -> MongoClient
MongoDriver.objectId(oid: string) -> ObjectId

MongoClient.ping() -> string
MongoClient.collection(database: string, collection: string) -> MongoCollection

MongoCollection.find(query: Dict) -> MongoCursor
MongoCollection.findOne(query: Dict, options: ?Dict) -> Dict|nil
MongoCollection.insertOne(obj: Dict) -> bool
MongoCollection.insertMany(objs: List[Dict]) -> bool
MongoCollection.updateOne(query: Dict, update: Dict) -> bool
MongoCollection.updateMany(query: Dict, update: Dict) -> bool
MongoCollection.deleteOne(query: Dict, update: Dict) -> bool
MongoCollection.deleteMany(query: Dict, update: Dict) -> bool
MongoCollection.countDocuments(query: Dict) -> Number
MongoCollection.estimateDocumentCount(query: Dict) -> Number

MongoCursor.toArray() -> List[Dict]
MongoCursor.limit(limit: Number) -> MongoCursor
MongoCursor.skip(skip: Number) -> MongoCursor
MongoCursor.project(projection: Dict) -> MongoCursor
MongoCursor.sort(sort: Dict) -> MongoCursor
MongoCursor.hasNext() -> bool
MongoCursor.next() -> Dict|nil

(next, toArray, hasNext) calls on MongoCursor makes the other functions not do anything.
```

# Examples
See the `tests` folder.