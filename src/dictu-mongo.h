#ifndef DICTU_MONGO_H
#define DICTU_MONGO_H

#include <dictu-include.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>

typedef struct {
   mongoc_uri_t *uri;
   mongoc_client_t *client;
} DictuMongoClient;

typedef struct {
 mongoc_collection_t *collection;
} DictuMongoCollection;

typedef struct {
 bool started;
 bool done;
 int skip;
 int limit;
 bson_t* query;
 bson_t* sort;
 bson_t* project;
 bson_t* opts;
 bson_t* out;
 mongoc_cursor_t * cursor;
 mongoc_collection_t* collection;
} DictuMongoCursor;

typedef struct {
   bson_oid_t id;
} DictuObjectId;

#define AS_MONGO_CLIENT(v) ((DictuMongoClient *)AS_ABSTRACT(v)->data)
#define AS_MONGO_COLLECTION(v) ((DictuMongoCollection *)AS_ABSTRACT(v)->data)
#define AS_MONGO_CURSOR(v) ((DictuMongoCursor *)AS_ABSTRACT(v)->data)
#define AS_MONGO_OBJECTID(v) ((DictuObjectId *)AS_ABSTRACT(v)->data)

bson_t* dictu_value_to_bson_dict(Value value, bson_t* in);
bson_array_builder_t* dictu_value_to_bson_list(Value value, bson_array_builder_t* in);

Value parse_bson_to_dictu(DictuVM *vm, bson_t* b, bool init);

bool dictu_mongo_init_start_find(DictuVM* vm, DictuMongoCursor* c);

void dissalocate(DictuVM* vm, DictuMongoCursor* c);

#endif