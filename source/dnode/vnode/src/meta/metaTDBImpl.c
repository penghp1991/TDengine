/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "metaDef.h"

#include "tdbInt.h"

struct SMetaDB {
  TENV *pEnv;
  TDB  *pTbDB;
  TDB  *pSchemaDB;
  TDB  *pNameIdx;
  TDB  *pStbIdx;
  TDB  *pNtbIdx;
  TDB  *pCtbIdx;
};

typedef struct __attribute__((__packed__)) {
  tb_uid_t uid;
  int32_t  sver;
} SSchemaDbKey;

typedef struct {
  char    *name;
  tb_uid_t uid;
} SNameIdxKey;

typedef struct {
  tb_uid_t suid;
  tb_uid_t uid;
} SCtbIdxKey;

static inline int metaUidCmpr(const void *arg1, int len1, const void *arg2, int len2) {
  tb_uid_t uid1, uid2;

  ASSERT(len1 == sizeof(tb_uid_t));
  ASSERT(len2 == sizeof(tb_uid_t));

  uid1 = ((tb_uid_t *)arg1)[0];
  uid2 = ((tb_uid_t *)arg2)[1];

  if (uid1 < uid2) {
    return -1;
  }
  if (uid1 == uid2) {
    return 0;
  } else {
    return 1;
  }
}

static inline int metaSchemaKeyCmpr(const void *arg1, int len1, const void *arg2, int len2) {
  int           c;
  SSchemaDbKey *pKey1 = (SSchemaDbKey *)arg1;
  SSchemaDbKey *pKey2 = (SSchemaDbKey *)arg2;

  c = metaUidCmpr(arg1, sizeof(tb_uid_t), arg2, sizeof(tb_uid_t));
  if (c) return c;

  if (pKey1->sver > pKey2->sver) {
    return 1;
  } else if (pKey1->sver == pKey2->sver) {
    return 0;
  } else {
    return -1;
  }
}

static inline int metaNameIdxCmpr(const void *arg1, int len1, const void *arg2, int len2) {
  return strcmp((char *)arg1, (char *)arg2);
}

static inline int metaCtbIdxCmpr(const void *arg1, int len1, const void *arg2, int len2) {
  int         c;
  SCtbIdxKey *pKey1 = (SCtbIdxKey *)arg1;
  SCtbIdxKey *pKey2 = (SCtbIdxKey *)arg2;

  c = metaUidCmpr(arg1, sizeof(tb_uid_t), arg2, sizeof(tb_uid_t));
  if (c) return c;

  return metaUidCmpr(&pKey1->uid, sizeof(tb_uid_t), &pKey2->uid, sizeof(tb_uid_t));
}

int metaOpenDB(SMeta *pMeta) {
  SMetaDB *pMetaDb;
  int      ret;

  // allocate DB handle
  pMetaDb = taosMemoryCalloc(1, sizeof(*pMetaDb));
  if (pMetaDb == NULL) {
    // TODO
    ASSERT(0);
    return -1;
  }

  // open the ENV
  ret = tdbEnvOpen(pMeta->path, 4096, 256, &(pMetaDb->pEnv));
  if (ret < 0) {
    // TODO
    ASSERT(0);
    return -1;
  }

  // open table DB
  ret = tdbDbOpen("table.db", sizeof(tb_uid_t), TDB_VARIANT_LEN, metaUidCmpr, pMetaDb->pEnv, &(pMetaDb->pTbDB));
  if (ret < 0) {
    // TODO
    ASSERT(0);
    return -1;
  }

  // open schema DB
  ret = tdbDbOpen("schema.db", sizeof(tb_uid_t) + sizeof(int32_t), TDB_VARIANT_LEN, metaSchemaKeyCmpr, pMetaDb->pEnv,
                  &(pMetaDb->pSchemaDB));
  if (ret < 0) {
    // TODO
    ASSERT(0);
    return -1;
  }

  ret = tdbDbOpen("name.idx", TDB_VARIANT_LEN, 0, metaNameIdxCmpr, pMetaDb->pEnv, &(pMetaDb->pNameIdx));
  if (ret < 0) {
    // TODO
    ASSERT(0);
    return -1;
  }

  ret = tdbDbOpen("stb.idx", sizeof(tb_uid_t), 0, metaUidCmpr, pMetaDb->pEnv, &(pMetaDb->pStbIdx));
  if (ret < 0) {
    // TODO
    ASSERT(0);
    return -1;
  }

  ret = tdbDbOpen("ntb.idx", sizeof(tb_uid_t), 0, metaUidCmpr, pMetaDb->pEnv, &(pMetaDb->pNtbIdx));
  if (ret < 0) {
    // TODO
    ASSERT(0);
    return -1;
  }

  ret = tdbDbOpen("ctb.idx", sizeof(tb_uid_t), 0, metaCtbIdxCmpr, pMetaDb->pEnv, &(pMetaDb->pCtbIdx));
  if (ret < 0) {
    // TODO
    ASSERT(0);
    return -1;
  }

  return 0;
}

void metaCloseDB(SMeta *pMeta) {
  if (pMeta->pDB) {
    tdbDbClose(pMeta->pDB->pCtbIdx);
    tdbDbClose(pMeta->pDB->pNtbIdx);
    tdbDbClose(pMeta->pDB->pStbIdx);
    tdbDbClose(pMeta->pDB->pNameIdx);
    tdbDbClose(pMeta->pDB->pSchemaDB);
    tdbDbClose(pMeta->pDB->pTbDB);
    taosMemoryFree(pMeta->pDB);
  }
}

int metaSaveTableToDB(SMeta *pMeta, STbCfg *pTbCfg) {
  tb_uid_t uid;
  SMetaDB *pMetaDb;
  void    *pKey;
  void    *pVal;
  int      kLen;
  int      vLen;
  int      ret;

  pMetaDb = pMeta->pDB;

  // TODO: make this operation pre-process
  if (pTbCfg->type == META_SUPER_TABLE) {
    uid = pTbCfg->stbCfg.suid;
  } else {
    uid = metaGenerateUid(pMeta);
  }

  // save to table.db
  ret = tdbDbInsert(pMetaDb->pTbDB, pKey, kLen, pVal, vLen);
  if (ret < 0) {
    return -1;
  }

  // save to schema.db
  ret = tdbDbInsert(pMetaDb->pSchemaDB, pKey, kLen, pVal, vLen);
  if (ret < 0) {
    return -1;
  }

  // update name.idx
  ret = tdbDbInsert(pMetaDb->pNameIdx, pKey, kLen, NULL, 0);
  if (ret < 0) {
    return -1;
  }

  if (pTbCfg->type == META_SUPER_TABLE) {
    ret = tdbDbInsert(pMetaDb->pStbIdx, pKey, kLen, NULL, 0);
    if (ret < 0) {
      return -1;
    }
  } else if (pTbCfg->type == META_CHILD_TABLE) {
    ret = tdbDbInsert(pMetaDb->pCtbIdx, pKey, kLen, NULL, 0);
    if (ret < 0) {
      return -1;
    }
  } else if (pTbCfg->type == META_NORMAL_TABLE) {
    ret = tdbDbInsert(pMetaDb->pNtbIdx, pKey, kLen, NULL, 0);
    if (ret < 0) {
      return -1;
    }
  }

  return 0;
}

int metaRemoveTableFromDb(SMeta *pMeta, tb_uid_t uid) {
  // TODO
  ASSERT(0);
  return 0;
}

STbCfg *metaGetTbInfoByUid(SMeta *pMeta, tb_uid_t uid) {
  // TODO
  ASSERT(0);
  return NULL;
}

STbCfg *metaGetTbInfoByName(SMeta *pMeta, char *tbname, tb_uid_t *uid) {
  // TODO
  ASSERT(0);
  return NULL;
}

SSchemaWrapper *metaGetTableSchema(SMeta *pMeta, tb_uid_t uid, int32_t sver, bool isinline) {
  // TODO
  ASSERT(0);
  return NULL;
}

STSchema *metaGetTbTSchema(SMeta *pMeta, tb_uid_t uid, int32_t sver) {
  // TODO
  ASSERT(0);
  return NULL;
}

SMTbCursor *metaOpenTbCursor(SMeta *pMeta) {
  // TODO
  ASSERT(0);
  return NULL;
}

void metaCloseTbCursor(SMTbCursor *pTbCur) {
  // TODO
  ASSERT(0);
}

char *metaTbCursorNext(SMTbCursor *pTbCur) {
  // TODO
  ASSERT(0);
  return NULL;
}

SMCtbCursor *metaOpenCtbCursor(SMeta *pMeta, tb_uid_t uid) {
  // TODO
  ASSERT(0);
  return NULL;
}

void metaCloseCtbCurosr(SMCtbCursor *pCtbCur) {
  // TODO
  ASSERT(0);
}

tb_uid_t metaCtbCursorNext(SMCtbCursor *pCtbCur) {
  // TODO
  ASSERT(0);
  return 0;
}

int metaGetTbNum(SMeta *pMeta) {
  // TODO
  // ASSERT(0);
  return 0;
}

STSmaWrapper *metaGetSmaInfoByTable(SMeta *pMeta, tb_uid_t uid) {
  // TODO
  ASSERT(0);
  return NULL;
}

int metaRemoveSmaFromDb(SMeta *pMeta, int64_t indexUid) {
  // TODO
  ASSERT(0);
  return 0;
}

int metaSaveSmaToDB(SMeta *pMeta, STSma *pSmaCfg) {
  // TODO
  ASSERT(0);
  return 0;
}

STSma *metaGetSmaInfoByIndex(SMeta *pMeta, int64_t indexUid) {
  // TODO
  ASSERT(0);
  return NULL;
}

const char *metaSmaCursorNext(SMSmaCursor *pCur) {
  // TODO
  ASSERT(0);
  return NULL;
}

void metaCloseSmaCurosr(SMSmaCursor *pCur) {
  // TODO
  ASSERT(0);
}

SArray *metaGetSmaTbUids(SMeta *pMeta, bool isDup) {
  // TODO
  ASSERT(0);
  return NULL;
}

SMSmaCursor *metaOpenSmaCursor(SMeta *pMeta, tb_uid_t uid) {
  // TODO
  ASSERT(0);
  return NULL;
}
