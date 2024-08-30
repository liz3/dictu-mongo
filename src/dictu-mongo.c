#include "dictu-mongo.h"
#include "bson/bson.h"
#include "dictu-include.h"
#include <stdint.h>
#include <string.h>
#include <time.h>

void freeMongoObjectId(DictuVM *vm, ObjAbstract *abstract) {
  DictuObjectId *id = (DictuObjectId *)abstract->data;
  // FREE_ARRAY(vm, uint8_t, buffer->bytes, buffer->size);
  FREE(vm, DictuObjectId, abstract->data);
}

char *mongoObjectIdToString(ObjAbstract *abstract) {
  UNUSED(abstract);

  char *bufferString = malloc(sizeof(char) * 11);
  snprintf(bufferString, 11, "<ObjectId>");
  return bufferString;
}

bool dictu_mongo_init_start_find(DictuVM *vm, DictuMongoCursor *c) {
  if (c->started) {
    return false;
  }
  c->started = true;
  c->opts = ALLOCATE(vm, bson_t, 1);
  bson_init(c->opts);
  bson_t *opts = (c->opts);
  if (c->limit != 0)
    BSON_APPEND_INT32(opts, "limit", c->limit);
  if (c->skip != 0)
    BSON_APPEND_INT32(opts, "skip", c->skip);
  if (c->sort != NULL)
    BSON_APPEND_DOCUMENT(opts, "sort", c->sort);
  if (c->project != NULL)
    BSON_APPEND_DOCUMENT(opts, "projection", c->project);
  c->cursor =
      mongoc_collection_find_with_opts(c->collection, c->query, c->opts, NULL);
  return true;
}

bson_t *dictu_value_to_bson_dict(Value value, bson_t *in) {
  if (in == NULL) {
    return in;
  }

  ObjDict *d = AS_DICT(value);

  for (int i = 0; i < d->capacityMask + 1; ++i) {
    if (IS_EMPTY(d->entries[i].key)) {
      continue;
    }
    ObjString *key = AS_STRING(d->entries[i].key);
    Value v = d->entries[i].value;
    if (IS_NUMBER(v)) {
      BSON_APPEND_INT64(in, key->chars, AS_NUMBER(v));
    } else if (IS_BOOL(v)) {
      BSON_APPEND_BOOL(in, key->chars, AS_BOOL(v));
    } else if (IS_NIL(v)) {
      BSON_APPEND_NULL(in, key->chars);
    } else if (IS_STRING(v)) {
      ObjString *str = AS_STRING(v);
      bson_append_utf8(in, key->chars, key->length, str->chars, str->length);
    } else if (IS_DICT(v)) {
      bson_t child;
      bson_append_document_begin(in, key->chars, key->length, &child);
      dictu_value_to_bson_dict(v, &child);
      bson_append_document_end(in, &child);
    } else if (IS_LIST(v)) {
      bson_array_builder_t *bab;
      bson_append_array_builder_begin(in, key->chars, key->length, &bab);
      dictu_value_to_bson_list(v, bab);
      bson_append_array_builder_end(in, bab);
    } else if (IS_ABSTRACT(v)) {
      ObjAbstract *abstract = AS_ABSTRACT(v);
      if (abstract->type == mongoObjectIdToString) {
        DictuObjectId *id = AS_MONGO_OBJECTID(v);
        bson_append_oid(in, key->chars, key->length, &(id->id));
      }
    }
  }

  return in;
}
bson_array_builder_t *dictu_value_to_bson_list(Value value,
                                               bson_array_builder_t *in) {
  if (in == NULL) {
    return in;
  }

  ObjList *d = AS_LIST(value);

  for (int i = 0; i < d->values.count; ++i) {

    Value v = d->values.values[i];
    if (IS_NUMBER(v)) {
      bson_array_builder_append_int64(in, AS_NUMBER(v));
    } else if (IS_BOOL(v)) {
      bson_array_builder_append_bool(in, AS_BOOL(v));
    } else if (IS_NIL(v)) {
      bson_array_builder_append_null(in);
    } else if (IS_STRING(v)) {
      ObjString *str = AS_STRING(v);
      bson_array_builder_append_utf8(in, str->chars, str->length);
    } else if (IS_DICT(v)) {
      bson_t child = BSON_INITIALIZER;
      dictu_value_to_bson_dict(v, &child);
      bson_array_builder_append_document(in, &child);
    } else if (IS_LIST(v)) {
      bson_array_builder_t *bab = bson_array_builder_new();
      bson_array_builder_append_array_builder_begin(in, &bab);
      dictu_value_to_bson_list(v, bab);
      bson_array_builder_append_array_builder_end(in, bab);
      bson_array_builder_destroy(bab);
    } else if (IS_ABSTRACT(v)) {
      ObjAbstract *abstract = AS_ABSTRACT(v);
      if (abstract->type == mongoObjectIdToString) {
        DictuObjectId *id = AS_MONGO_OBJECTID(v);
        bson_array_builder_append_oid(in, &(id->id));
      }
    }
  }

  return in;
}

Value parse_bson_to_dictu(DictuVM *vm, bson_t *b, bool isList) {
  bson_iter_t iter;
  if (bson_iter_init(&iter, b)) {
    // char* json = bson_as_canonical_extended_json (b, NULL);
    // printf("%s\n\n", json);
    if (!isList) {
      ObjDict *dict = newDict(vm);
      push(vm, OBJ_VAL(dict));
      while (bson_iter_next(&iter)) {
        bson_type_t t = bson_iter_type(&iter);
        Value key = OBJ_VAL(
            copyString(vm, bson_iter_key(&iter), bson_iter_key_len(&iter)));
        if (t == BSON_TYPE_UTF8) {
          uint32_t len;
          const char *data = bson_iter_utf8(&iter, &len);
          dictSet(vm, dict, key, OBJ_VAL(copyString(vm, data, len)));

          // bson_free(data);
        } else if (t == BSON_TYPE_INT32) {
          dictSet(vm, dict, key, NUMBER_VAL(bson_iter_int32(&iter)));
        } else if (t == BSON_TYPE_INT64) {
          dictSet(vm, dict, key, NUMBER_VAL(bson_iter_int64(&iter)));
        } else if (t == BSON_TYPE_DOUBLE) {
          dictSet(vm, dict, key, NUMBER_VAL(bson_iter_double(&iter)));
        } else if (t == BSON_TYPE_DATE_TIME) {
          dictSet(vm, dict, key, NUMBER_VAL(bson_iter_date_time(&iter)));
        } else if (t == BSON_TYPE_BOOL) {
          dictSet(vm, dict, key, BOOL_VAL(bson_iter_bool(&iter)));
        } else if (t == BSON_TYPE_NULL) {
          dictSet(vm, dict, key, NIL_VAL);
        } else if (t == BSON_TYPE_OID) {
          const bson_oid_t *id = bson_iter_oid(&iter);
          char str[25];
          bson_oid_to_string(id, str);
          dictSet(vm, dict, key, OBJ_VAL(copyString(vm, str, 24)));
        } else if (t == BSON_TYPE_ARRAY) {
          uint32_t len;
          uint8_t *data;
          bson_iter_array(&iter, &len, (const uint8_t **)&data);
          bson_t *child = bson_new_from_data(data, len);
          dictSet(vm, dict, key, parse_bson_to_dictu(vm, child, true));
          bson_destroy(child);
        } else if (t == BSON_TYPE_DOCUMENT) {
          uint32_t len;
          uint8_t *data;
          bson_iter_document(&iter, &len, (const uint8_t **)&data);
          bson_t *child = bson_new_from_data(data, len);
          dictSet(vm, dict, key, parse_bson_to_dictu(vm, child, false));
          bson_destroy(child);
        }
      }
      pop(vm);
      return OBJ_VAL(dict);
    } else {
      ObjList *list = newList(vm);
      push(vm, OBJ_VAL(list));
      while (bson_iter_next(&iter)) {
        bson_type_t t = bson_iter_type(&iter);
        if (t == BSON_TYPE_UTF8) {
          uint32_t len;
          const char *data = bson_iter_utf8(&iter, &len);
          writeValueArray(vm, &list->values,
                          OBJ_VAL(copyString(vm, data, len)));
          // bson_free(data);
        } else if (t == BSON_TYPE_INT32) {
          writeValueArray(vm, &list->values,
                          NUMBER_VAL(bson_iter_int32(&iter)));
        } else if (t == BSON_TYPE_INT64) {
          writeValueArray(vm, &list->values,
                          NUMBER_VAL(bson_iter_int64(&iter)));
        } else if (t == BSON_TYPE_DOUBLE) {
          writeValueArray(vm, &list->values,
                          NUMBER_VAL(bson_iter_double(&iter)));
        } else if (t == BSON_TYPE_DATE_TIME) {
          writeValueArray(vm, &list->values,
                          NUMBER_VAL(bson_iter_date_time(&iter)));
        } else if (t == BSON_TYPE_OID) {
          const bson_oid_t *id = bson_iter_oid(&iter);
          char str[25];
          bson_oid_to_string(id, str);
          writeValueArray(vm, &list->values, OBJ_VAL(copyString(vm, str, 24)));
        } else if (t == BSON_TYPE_BOOL) {
          writeValueArray(vm, &list->values, NUMBER_VAL(bson_iter_bool(&iter)));
        } else if (t == BSON_TYPE_NULL) {
          writeValueArray(vm, &list->values, NIL_VAL);
        } else if (t == BSON_TYPE_ARRAY) {
          uint32_t len;
          uint8_t *data;
          bson_iter_array(&iter, &len, (const uint8_t **)&data);
          bson_t *child = bson_new_from_data(data, len);
          writeValueArray(vm, &list->values,
                          parse_bson_to_dictu(vm, child, true));
          bson_destroy(child);
        } else if (t == BSON_TYPE_DOCUMENT) {
          uint32_t len;
          uint8_t *data;
          bson_iter_document(&iter, &len, (const uint8_t **)&data);
          bson_t *child = bson_new_from_data(data, len);
          writeValueArray(vm, &list->values,
                          parse_bson_to_dictu(vm, child, false));
          bson_destroy(child);
        }
      }
      pop(vm);
      return OBJ_VAL(list);
    }
  } else {
    printf("failed to init iter\n");
  }
  return NIL_VAL;
}

void freeMongoClient(DictuVM *vm, ObjAbstract *abstract) {
  DictuMongoClient *client = (DictuMongoClient *)abstract->data;
  mongoc_uri_destroy (client->uri);
  mongoc_client_destroy (client->client);
  
  // FREE_ARRAY(vm, uint8_t, buffer->bytes, buffer->size);
  FREE(vm, DictuMongoClient, abstract->data);
}

char *mongoClientToString(ObjAbstract *abstract) {
  UNUSED(abstract);

  char *bufferString = malloc(sizeof(char) * 14);
  snprintf(bufferString, 14, "<MongoClient>");
  return bufferString;
}

void freeMongoCollection(DictuVM *vm, ObjAbstract *abstract) {
  DictuMongoCollection *collection = (DictuMongoCollection *)abstract->data;
  mongoc_collection_destroy (collection->collection);
  // FREE_ARRAY(vm, uint8_t, buffer->bytes, buffer->size);
  FREE(vm, DictuMongoCollection, abstract->data);
}

char *mongoCollectionToString(ObjAbstract *abstract) {
  UNUSED(abstract);

  char *bufferString = malloc(sizeof(char) * 18);
  snprintf(bufferString, 18, "<MongoCollection>");
  return bufferString;
}

void freeMongoCursor(DictuVM *vm, ObjAbstract *abstract) {
  DictuMongoCursor *collection = (DictuMongoCursor *)abstract->data;
  if(collection->cursor) {
    mongoc_cursor_destroy(collection->cursor);
  }
  // FREE_ARRAY(vm, uint8_t, buffer->bytes, buffer->size);
  FREE(vm, DictuMongoCursor, abstract->data);
}

char *mongoCursorToString(ObjAbstract *abstract) {
  UNUSED(abstract);

  char *bufferString = malloc(sizeof(char) * 14);
  snprintf(bufferString, 14, "<MongoCursor>");
  return bufferString;
}

static Value dictuMongoCursorToArray(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCursor *c = AS_MONGO_CURSOR(args[0]);
  if (c->started) {
    return NIL_VAL;
  }
  dictu_mongo_init_start_find(vm, c);
  ObjList *list = newList(vm);
  push(vm, OBJ_VAL(list));
  bson_t *doc;
  while (mongoc_cursor_next(c->cursor, (const bson_t**)&doc)) {
    Value v = parse_bson_to_dictu(vm, doc, false);
    writeValueArray(vm, &list->values, v);
  }
  pop(vm);
  dissalocate(vm, c);
  return OBJ_VAL(list);
}

void dissalocate(DictuVM *vm, DictuMongoCursor *c) {
  mongoc_cursor_destroy(c->cursor);
  c->cursor = NULL;
  if (c->query) {
    bson_destroy(c->query);
    FREE(vm, bson_t, c->query);
    c->query = NULL;
  }
  if (c->opts) {
    bson_destroy(c->opts);
    FREE(vm, bson_t, c->opts);
    c->opts = NULL;
  }
  if (c->sort) {
    bson_destroy(c->sort);
    FREE(vm, bson_t, c->sort);
    c->sort = NULL;
  }
  if (c->out) {
    bson_destroy(c->out);
    FREE(vm, bson_t, c->out);
    c->out = NULL;
  }
  if (c->project) {
    bson_destroy(c->project);
    FREE(vm, bson_t, c->project);
    c->project = NULL;
  }
}
static Value dictuMongoCursorHasNext(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCursor *c = AS_MONGO_CURSOR(args[0]);
  if (c->done)
    return BOOL_VAL(false);
  dictu_mongo_init_start_find(vm, c);
  bool out = mongoc_cursor_next(c->cursor, (const bson_t**)&(c->out));
  c->done = !out;
  return BOOL_VAL(out);
}

static Value dictuMongoCursorLimit(DictuVM *vm, int argCount, Value *args) {
  if (argCount != 1 || !IS_NUMBER(args[1]))
    return NIL_VAL;
  DictuMongoCursor *c = AS_MONGO_CURSOR(args[0]);
  if (!c->started)
    c->limit = AS_NUMBER(args[1]);
  return args[0];
}
static Value dictuMongoCursorSkip(DictuVM *vm, int argCount, Value *args) {
  if (argCount != 1 || !IS_NUMBER(args[1]))
    return NIL_VAL;
  DictuMongoCursor *c = AS_MONGO_CURSOR(args[0]);
  if (!c->started)
    c->skip = AS_NUMBER(args[1]);
  return args[0];
}

static Value dictuMongoCursorNext(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCursor *c = AS_MONGO_CURSOR(args[0]);
  if (c->done)
    return BOOL_VAL(false);
  dictu_mongo_init_start_find(vm, c);
  if (c->out == NULL)
    mongoc_cursor_next(c->cursor, (const bson_t**)&(c->out));
  if (c->out == NULL)
    return NIL_VAL;
  return parse_bson_to_dictu(vm, c->out, false);
}

static Value dictuMongoCursorSort(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCursor *c = AS_MONGO_CURSOR(args[0]);
  if (c->started)
    return args[0];
  if (argCount == 1 && IS_DICT(args[1])) {
    if (c->sort) {
      bson_destroy(c->sort);
      FREE(vm, bson_t, c->sort);
    }
    c->sort = ALLOCATE(vm, bson_t, 1);
    bson_init(c->sort);
    dictu_value_to_bson_dict(args[1], c->sort);
  }
  return args[0];
}

static Value dictuMongoCursorProject(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCursor *c = AS_MONGO_CURSOR(args[0]);
  if (c->started)
    return args[0];
  if (argCount == 1 && IS_DICT(args[1])) {
    if (c->project) {
      bson_destroy(c->project);
      FREE(vm, bson_t, c->project);
    }
    c->project = ALLOCATE(vm, bson_t, 1);
    bson_init(c->project);
    dictu_value_to_bson_dict(args[1], c->project);
  }
  return args[0];
}

static Value dictuMongoFind(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);
  ObjAbstract *abstract = newAbstract(vm, freeMongoCursor, mongoCursorToString);
  push(vm, OBJ_VAL(abstract));
  DictuMongoCursor *cursor = ALLOCATE(vm, DictuMongoCursor, 1);
  defineNative(vm, &abstract->values, "toArray", dictuMongoCursorToArray);
  defineNative(vm, &abstract->values, "limit", dictuMongoCursorLimit);
  defineNative(vm, &abstract->values, "skip", dictuMongoCursorSkip);
  defineNative(vm, &abstract->values, "hasNext", dictuMongoCursorHasNext);
  defineNative(vm, &abstract->values, "next", dictuMongoCursorNext);
  defineNative(vm, &abstract->values, "project", dictuMongoCursorProject);
  defineNative(vm, &abstract->values, "sort", dictuMongoCursorSort);

  cursor->done = false;
  cursor->started = false;
  cursor->limit = 0;
  cursor->skip = 0;
  cursor->query = NULL;
  cursor->out = NULL;
  cursor->sort = NULL;
  cursor->project = NULL;
  cursor->opts = NULL;
  cursor->cursor = NULL;
  cursor->collection = client->collection;
  if (argCount == 1 && IS_DICT(args[1])) {
    cursor->query = ALLOCATE(vm, bson_t, 1);
    bson_init(cursor->query);
    dictu_value_to_bson_dict(args[1], cursor->query);
  }

  abstract->data = cursor;
  pop(vm);
  return OBJ_VAL(abstract);
}
static Value dictuMongoFindOne(DictuVM *vm, int argCount, Value *args) {
  if (argCount < 1 || !IS_DICT(args[1]))
    return NIL_VAL;
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);
  DictuMongoCursor cursor;
  cursor.started = false;
  cursor.done = false;
  cursor.collection = client->collection;
  cursor.sort = NULL;
  cursor.cursor = NULL;
  cursor.project = NULL;
  cursor.out = NULL;
  cursor.skip = 0;
  cursor.limit = 1;
  bson_t query = BSON_INITIALIZER;
  bson_t sort = BSON_INITIALIZER;
  bson_t project = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &query);
  cursor.query = &query;
  if (argCount > 1 && IS_DICT(args[2])) {
    ObjDict *d = AS_DICT(args[2]);
    for (int i = 0; i < d->capacityMask + 1; ++i) {
      if (IS_EMPTY(d->entries[i].key)) {
        continue;
      }
      ObjString *key = AS_STRING(d->entries[i].key);
      Value v = d->entries[i].value;
      if (strcmp(key->chars, "sort") == 0 && IS_DICT(v)) {
        dictu_value_to_bson_dict(v, &sort);
        cursor.sort = &sort;
      } else if (strcmp(key->chars, "project") == 0 && IS_DICT(v)) {
        dictu_value_to_bson_dict(v, &project);
        cursor.project = &project;
      } else if (strcmp(key->chars, "skip") == 0 && IS_NUMBER(v)) {
        cursor.skip = AS_NUMBER(v);
      }
    }
  }
  dictu_mongo_init_start_find(vm, &cursor);
  bool out = mongoc_cursor_next(cursor.cursor, (const bson_t**)&(cursor.out));
  Value res;
  if(!out)
    res=  BOOL_VAL(false);
  else
    res = parse_bson_to_dictu(vm, cursor.out, false);
  mongoc_cursor_destroy(cursor.cursor);
  FREE(vm, bson_t, cursor.opts);
  return res;
}
static Value dictuMongoInsertOne(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);
  if (argCount < 1 || !IS_DICT(args[1]))
    return NIL_VAL;
  bson_t doc = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &doc);
  bson_error_t error;
  bool res = mongoc_collection_insert_one(client->collection, &doc, NULL, NULL,
                                          &error);

  return BOOL_VAL(res);
}
static Value dictuMongoInsertMany(DictuVM *vm, int argCount, Value *args) {
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);
  if (argCount < 1 || !IS_LIST(args[1]))
    return NIL_VAL;
  ObjList *list = AS_LIST(args[1]);
  bson_t *docs = ALLOCATE(vm, bson_t, list->values.count);
  for (size_t i = 0; i < list->values.count; i++) {
    bson_init(&docs[i]);
    dictu_value_to_bson_dict(list->values.values[i], &docs[i]);
  }

  bson_error_t error;
  bool res = mongoc_collection_insert_many(
      client->collection, &docs, list->values.count, NULL, NULL, &error);
  FREE_ARRAY(vm, bson_t, docs, list->values.count);
  return BOOL_VAL(res);
}

static Value dictuMongoCountDocuments(DictuVM *vm, int argCount, Value *args) {
  if (argCount == 0)
    return NIL_VAL;
  if (!IS_DICT(args[1]))
    return NIL_VAL;
  bson_t query = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &query);
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);

  bson_error_t error;
  int64_t count;
  int32_t skip = 0;
  int32_t limit = 0;
  if (argCount >= 2)
    skip = AS_NUMBER(args[2]);
  if (argCount >= 3)
    limit = AS_NUMBER(args[3]);
  count = mongoc_collection_count(client->collection, MONGOC_QUERY_NONE, &query,
                                  skip, limit, NULL, &error);

  return NUMBER_VAL(count);
}

static Value dictuMongoEstimateDocumentCount(DictuVM *vm, int argCount,
                                             Value *args) {
  if (argCount == 0)
    return NIL_VAL;
  if (!IS_DICT(args[1]))
    return NIL_VAL;
  bson_t query = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &query);
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);

  bson_error_t error;
  int64_t count;
  count = mongoc_collection_estimated_document_count(client->collection, &query,
                                                     NULL, NULL, &error);

  return NUMBER_VAL(count);
}

static Value dictuMongoDeleteOne(DictuVM *vm, int argCount, Value *args) {
  if (argCount == 0)
    return NIL_VAL;
  if (!IS_DICT(args[1]))
    return NIL_VAL;
  bson_t query = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &query);
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);

  bson_error_t error;
  bool result = mongoc_collection_delete_one(client->collection, &query, NULL,
                                             NULL, &error);

  return BOOL_VAL(result);
}

static Value dictuMongoDeleteMany(DictuVM *vm, int argCount, Value *args) {
  if (argCount == 0)
    return NIL_VAL;
  if (!IS_DICT(args[1]))
    return NIL_VAL;
  bson_t query = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &query);
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);

  bson_error_t error;
  bool result = mongoc_collection_delete_many(client->collection, &query, NULL,
                                              NULL, &error);

  return BOOL_VAL(result);
}

static Value dictuMongoUpdateOne(DictuVM *vm, int argCount, Value *args) {
  if (argCount < 2)
    return NIL_VAL;
  if (!IS_DICT(args[1]) || !IS_DICT(args[2]))
    return NIL_VAL;
  bson_t query = BSON_INITIALIZER;
  bson_t update = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &query);
  dictu_value_to_bson_dict(args[2], &update);
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);

  bson_error_t error;
  bool result = mongoc_collection_update_one(client->collection, &query,
                                             &update, NULL, NULL, &error);

  return BOOL_VAL(result);
}

static Value dictuMongoUpdateMany(DictuVM *vm, int argCount, Value *args) {
  if (argCount < 2)
    return NIL_VAL;
  if (!IS_DICT(args[1]) || !IS_DICT(args[2]))
    return NIL_VAL;
  bson_t query = BSON_INITIALIZER;
  bson_t update = BSON_INITIALIZER;
  dictu_value_to_bson_dict(args[1], &query);
  dictu_value_to_bson_dict(args[2], &update);
  DictuMongoCollection *client = AS_MONGO_COLLECTION(args[0]);

  bson_error_t error;
  bool result = mongoc_collection_update_many(client->collection, &query,
                                              &update, NULL, NULL, &error);

  return BOOL_VAL(result);
}

static Value dictuMongoCollection(DictuVM *vm, int argCount, Value *args) {
  if (argCount != 2 || !IS_STRING(args[1]) || !IS_STRING(args[2])) {
    return NUMBER_VAL(argCount);
  }
  DictuMongoClient *client = AS_MONGO_CLIENT(args[0]);
  ObjString *database_str = AS_STRING(args[1]);
  ObjString *collection_str = AS_STRING(args[2]);
  ObjAbstract *abstract =
      newAbstract(vm, freeMongoCollection, mongoCollectionToString);
  push(vm, OBJ_VAL(abstract));

  DictuMongoCollection *collection_ref = ALLOCATE(vm, DictuMongoCollection, 1);
  collection_ref->collection = mongoc_client_get_collection(
      client->client, database_str->chars, collection_str->chars);
  defineNative(vm, &abstract->values, "find", dictuMongoFind);
  defineNative(vm, &abstract->values, "findOne", dictuMongoFindOne);
  defineNative(vm, &abstract->values, "insertOne", dictuMongoInsertOne);
  defineNative(vm, &abstract->values, "insertMany", dictuMongoInsertMany);
  defineNative(vm, &abstract->values, "countDocuments",
               dictuMongoCountDocuments);
  defineNative(vm, &abstract->values, "estimateDocumentCount",
               dictuMongoEstimateDocumentCount);
  defineNative(vm, &abstract->values, "deleteOne", dictuMongoDeleteOne);
  defineNative(vm, &abstract->values, "deleteMany", dictuMongoDeleteMany);
  defineNative(vm, &abstract->values, "updateOne", dictuMongoUpdateOne);
  defineNative(vm, &abstract->values, "updateMany", dictuMongoUpdateMany);

  abstract->data = collection_ref;
  pop(vm);
  return OBJ_VAL(abstract);
}
static Value dictuMongoDatabsse(DictuVM *vm, int argCount, Value *args) {

  return NIL_VAL;
}

static Value dictuMongoPing(DictuVM *vm, int argCount, Value *args) {
  bson_error_t error;
  DictuMongoClient *client = AS_MONGO_CLIENT(args[0]);
  bson_t *command, reply;
  bool retval;
  command = BCON_NEW("ping", BCON_INT32(1));
  retval = mongoc_client_command_simple(client->client, "admin", command, NULL,
                                        &reply, &error);
  if (!retval) {
    return OBJ_VAL(copyString(vm, error.message, strlen(error.message)));
  }
  char *str = bson_as_json(&reply, NULL);
  Value out = OBJ_VAL(copyString(vm, str, strlen(str)));
  bson_destroy(&reply);
  bson_destroy(command);
  bson_free(str);

  return out;
}
Value dictu_mongo_object_id(DictuVM *vm, int argCount, Value *args) {
  if (argCount != 1 || !IS_STRING(args[0])) {
    return NIL_VAL;
  }
  ObjString *str = AS_STRING(args[0]);
  if (str->length != 24)
    return NIL_VAL;
  ObjAbstract *abstract =
      newAbstract(vm, freeMongoObjectId, mongoObjectIdToString);
  push(vm, OBJ_VAL(abstract));

  DictuObjectId *id = ALLOCATE(vm, DictuObjectId, 1);
  bson_oid_init_from_string(&(id->id), str->chars);
  abstract->data = id;
  pop(vm);

  return OBJ_VAL(abstract);
}

Value dictu_mongo_create_client(DictuVM *vm, int argCount, Value *args) {
  if (argCount != 1 || !IS_STRING(args[0])) {
    return NIL_VAL;
  }
  bson_error_t error;
  ObjString *url = AS_STRING(args[0]);
  ObjAbstract *abstract = newAbstract(vm, freeMongoClient, mongoClientToString);
  push(vm, OBJ_VAL(abstract));
  DictuMongoClient *dictuClient = ALLOCATE(vm, DictuMongoClient, 1);
  dictuClient->uri = mongoc_uri_new_with_error(url->chars, &error);
  dictuClient->client = mongoc_client_new_from_uri(dictuClient->uri);
  mongoc_client_set_appname(dictuClient->client, "mongo-dictu-v2");

  defineNative(vm, &abstract->values, "collection", dictuMongoCollection);
  defineNative(vm, &abstract->values, "ping", dictuMongoPing);
  defineNative(vm, &abstract->values, "database", dictuMongoDatabsse);
  abstract->data = dictuClient;
  pop(vm);

  return OBJ_VAL(abstract);
}

int dictu_ffi_init(DictuVM *vm, Table *method_table) {
  mongoc_init();
  defineNative(vm, method_table, "createClient", dictu_mongo_create_client);
  defineNative(vm, method_table, "objectId", dictu_mongo_object_id);

  return 0;
}