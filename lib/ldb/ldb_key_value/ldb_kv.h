#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "tdb.h"
#include "ldb_module.h"

struct ldb_kv_private;
typedef int (*ldb_kv_traverse_fn)(struct ldb_kv_private *ldb_kv,
				  struct ldb_val key,
				  struct ldb_val data,
				  void *ctx);

struct kv_db_ops {
	int (*store)(struct ldb_kv_private *ldb_kv,
		     struct ldb_val key,
		     struct ldb_val data,
		     int flags);
	int (*delete)(struct ldb_kv_private *ldb_kv, struct ldb_val key);
	int (*iterate)(struct ldb_kv_private *ldb_kv,
		       ldb_kv_traverse_fn fn,
		       void *ctx);
	int (*update_in_iterate)(struct ldb_kv_private *ldb_kv,
				 struct ldb_val key,
				 struct ldb_val key2,
				 struct ldb_val data,
				 void *ctx);
	int (*fetch_and_parse)(struct ldb_kv_private *ldb_kv,
			       struct ldb_val key,
			       int (*parser)(struct ldb_val key,
					     struct ldb_val data,
					     void *private_data),
			       void *ctx);
	int (*lock_read)(struct ldb_module *);
	int (*unlock_read)(struct ldb_module *);
	int (*begin_write)(struct ldb_kv_private *);
	int (*prepare_write)(struct ldb_kv_private *);
	int (*abort_write)(struct ldb_kv_private *);
	int (*finish_write)(struct ldb_kv_private *);
	int (*error)(struct ldb_kv_private *ldb_kv);
	const char *(*errorstr)(struct ldb_kv_private *ldb_kv);
	const char *(*name)(struct ldb_kv_private *ldb_kv);
	bool (*has_changed)(struct ldb_kv_private *ldb_kv);
	bool (*transaction_active)(struct ldb_kv_private *ldb_kv);
};

/* this private structure is used by the key value backends in the
   ldb_context */
struct ldb_kv_private {
	const struct kv_db_ops *kv_ops;
	struct ldb_module *module;
	TDB_CONTEXT *tdb;
	struct lmdb_private *lmdb_private;
	unsigned int connect_flags;

	unsigned long long sequence_number;

	/* the low level tdb seqnum - used to avoid loading BASEINFO when
	   possible */
	int tdb_seqnum;

	struct ldb_kv_cache {
		struct ldb_message *indexlist;
		bool one_level_indexes;
		bool attribute_indexes;
		const char *GUID_index_attribute;
		const char *GUID_index_dn_component;
	} *cache;


	bool check_base;
	bool disallow_dn_filter;
	struct ldb_kv_idxptr *idxptr;
	bool prepared_commit;
	int read_lock_count;

	bool warn_unindexed;
	bool warn_reindex;

	bool read_only;

	bool reindex_failed;

	const struct ldb_schema_syntax *GUID_index_syntax;

	/*
	 * Maximum index key length.  If non zero keys longer than this length
	 * will be truncated for non unique indexes. Keys for unique indexes
	 * greater than this length will be rejected.
	 */
	unsigned max_key_length;

	/*
	 * To allow testing that ensures the DB does not fall back
	 * to a full scan
	 */
	bool disable_full_db_scan;

	/*
	 * The PID that opened this database so we don't work in a
	 * fork()ed child.
	 */
	pid_t pid;
};

struct ldb_kv_context {
	struct ldb_module *module;
	struct ldb_request *req;

	bool request_terminated;
	struct ldb_kv_req_spy *spy;

	/* search stuff */
	const struct ldb_parse_tree *tree;
	struct ldb_dn *base;
	enum ldb_scope scope;
	const char * const *attrs;
	struct tevent_timer *timeout_event;

	/* error handling */
	int error;
};

struct ldb_kv_reindex_context {
	struct ldb_module *module;
	int error;
	uint32_t count;
};


/* special record types */
#define LDB_KV_INDEX      "@INDEX"
#define LDB_KV_INDEXLIST  "@INDEXLIST"
#define LDB_KV_IDX        "@IDX"
#define LDB_KV_IDXVERSION "@IDXVERSION"
#define LDB_KV_IDXATTR    "@IDXATTR"
#define LDB_KV_IDXONE     "@IDXONE"
#define LDB_KV_IDXDN     "@IDXDN"
#define LDB_KV_IDXGUID    "@IDXGUID"
#define LDB_KV_IDX_DN_GUID "@IDX_DN_GUID"

/*
 * This will be used to indicate when a new, yet to be developed
 * sub-database version of the indicies are in use, to ensure we do
 * not load future databases unintentionally.
 */

#define LDB_KV_IDX_LMDB_SUBDB "@IDX_LMDB_SUBDB"

#define LDB_KV_BASEINFO   "@BASEINFO"
#define LDB_KV_OPTIONS    "@OPTIONS"
#define LDB_KV_ATTRIBUTES "@ATTRIBUTES"

/* special attribute types */
#define LDB_KV_SEQUENCE_NUMBER "sequenceNumber"
#define LDB_KV_CHECK_BASE "checkBaseOnSearch"
#define LDB_KV_DISALLOW_DN_FILTER "disallowDNFilter"
#define LDB_KV_MOD_TIMESTAMP "whenChanged"
#define LDB_KV_OBJECTCLASS "objectClass"

/* DB keys */
#define LDB_KV_GUID_KEY_PREFIX "GUID="
#define LDB_KV_GUID_SIZE 16
#define LDB_KV_GUID_KEY_SIZE (LDB_KV_GUID_SIZE + sizeof(LDB_KV_GUID_KEY_PREFIX) - 1)

/*
 * The following definitions come from lib/ldb/ldb_key_value/ldb_kv_cache.c
 */

int ldb_kv_cache_reload(struct ldb_module *module);
int ldb_kv_cache_load(struct ldb_module *module);
int ldb_kv_increase_sequence_number(struct ldb_module *module);
int ldb_kv_check_at_attributes_values(const struct ldb_val *value);

/*
 * The following definitions come from lib/ldb/ldb_key_value/ldb_kv_index.c
 */

struct ldb_parse_tree;

int ldb_kv_search_indexed(struct ldb_kv_context *ctx, uint32_t *);
int ldb_kv_index_add_new(struct ldb_module *module,
			 struct ldb_kv_private *ldb_kv,
			 const struct ldb_message *msg);
int ldb_kv_index_delete(struct ldb_module *module,
			const struct ldb_message *msg);
int ldb_kv_index_del_element(struct ldb_module *module,
			     struct ldb_kv_private *ldb_kv,
			     const struct ldb_message *msg,
			     struct ldb_message_element *el);
int ldb_kv_index_add_element(struct ldb_module *module,
			     struct ldb_kv_private *ldb_kv,
			     const struct ldb_message *msg,
			     struct ldb_message_element *el);
int ldb_kv_index_del_value(struct ldb_module *module,
			   struct ldb_kv_private *ldb_kv,
			   const struct ldb_message *msg,
			   struct ldb_message_element *el,
			   unsigned int v_idx);
int ldb_kv_reindex(struct ldb_module *module);
int ldb_kv_index_transaction_start(struct ldb_module *module);
int ldb_kv_index_transaction_commit(struct ldb_module *module);
int ldb_kv_index_transaction_cancel(struct ldb_module *module);
int ldb_kv_key_dn_from_idx(struct ldb_module *module,
			   struct ldb_kv_private *ldb_kv,
			   TALLOC_CTX *mem_ctx,
			   struct ldb_dn *dn,
			  struct ldb_val *key);

/*
 * The following definitions come from lib/ldb/ldb_key_value/ldb_kv_search.c
 */
int ldb_kv_search_dn1(struct ldb_module *module,
		      struct ldb_dn *dn,
		      struct ldb_message *msg,
		      unsigned int unpack_flags);
int ldb_kv_search_base(struct ldb_module *module,
		       TALLOC_CTX *mem_ctx,
		       struct ldb_dn *dn,
		       struct ldb_dn **ret_dn);
int ldb_kv_search_key(struct ldb_module *module,
		      struct ldb_kv_private *ldb_kv,
		      const struct ldb_val ldb_key,
		      struct ldb_message *msg,
		      unsigned int unpack_flags);
int ldb_kv_filter_attrs(TALLOC_CTX *mem_ctx,
			const struct ldb_message *msg,
			const char *const *attrs,
			struct ldb_message **filtered_msg);
int ldb_kv_search(struct ldb_kv_context *ctx);

/*
 * The following definitions come from lib/ldb/ldb_key_value/ldb_kv.c  */
/*
 * Determine if this key could hold a normal record.  We allow the new
 * GUID index, the old DN index and a possible future ID= but not
 * DN=@.
 */
bool ldb_kv_key_is_normal_record(struct ldb_val key);
struct ldb_val ldb_kv_key_dn(struct ldb_module *module,
			     TALLOC_CTX *mem_ctx,
			     struct ldb_dn *dn);
struct ldb_val ldb_kv_key_msg(struct ldb_module *module,
			     TALLOC_CTX *mem_ctx,
			      const struct ldb_message *msg);
int ldb_kv_guid_to_key(struct ldb_module *module,
		       struct ldb_kv_private *ldb_kv,
		       const struct ldb_val *GUID_val,
		       struct ldb_val *key);
int ldb_kv_idx_to_key(struct ldb_module *module,
		      struct ldb_kv_private *ldb_kv,
		      TALLOC_CTX *mem_ctx,
		      const struct ldb_val *idx_val,
		      struct ldb_val *key);
int ldb_kv_store(struct ldb_module *module,
		 const struct ldb_message *msg,
		 int flgs);
int ldb_kv_modify_internal(struct ldb_module *module,
			   const struct ldb_message *msg,
			   struct ldb_request *req);
int ldb_kv_delete_noindex(struct ldb_module *module,
			  const struct ldb_message *msg);
int ldb_kv_init_store(struct ldb_kv_private *ldb_kv,
		      const char *name,
		      struct ldb_context *ldb,
		      const char *options[],
		      struct ldb_module **_module);
