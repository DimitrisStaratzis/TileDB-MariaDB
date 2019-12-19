/**
 * @file   ha_mytile.h
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017-2019 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * This is the main handler implementation
 */

#pragma once

#ifdef USE_PRAGMA_INTERFACE
#pragma interface /* gcc class implementation */
#endif

#include "ha_mytile_share.h"
#include "mytile-buffer.h"
#include "mytile-range.h"
#include "mytile-sysvars.h"
#include <handler.h>
#include <memory>
#include <tiledb/tiledb>

#include "handler.h"   /* handler */
#include "my_base.h"   /* ha_rows */
#include "my_global.h" /* ulonglong */
#include "thr_lock.h"  /* THR_LOCK, THR_LOCK_DATA */

#define MYSQL_SERVER 1 // required for THD class

// Handler for mytile engine
extern handlerton *mytile_hton;
namespace tile {

class mytile : public handler {
public:
  /**
   * Main handler
   * @param hton
   * @param table_arg
   */
  mytile(handlerton *hton, TABLE_SHARE *table_arg);

  ~mytile() noexcept(true){};

  /**
   * flags for supported table features
   * @return
   */
  ulonglong table_flags(void) const override;

  /**
   * Create table
   * @param name
   * @param table_arg
   * @param create_info
   * @return
   */
  int create(const char *name, TABLE *table_arg,
             HA_CREATE_INFO *create_info) override;

  /**
   * Create array functionality
   * @param name
   * @param table_arg
   * @param create_info
   * @param context
   * @return
   */
  int create_array(const char *name, TABLE *table_arg,
                   HA_CREATE_INFO *create_info, tiledb::Context context);

  /**
   * Drop a table
   * note: we implement drop_table not delete_table because in drop_table the
   * table is open
   * @param name
   * @return
   */
  void drop_table(const char *name) override;

  /**
   * Drop a table by rm'ing the tiledb directory if mytile_delete_arrays=1 is
   * set. If mytile_delete_arrays is not set we just deregister the table
   *
   * @param name
   * @return
   */
  int delete_table(const char *name) override;

  /**
   * Open array
   * @param name
   * @param mode
   * @param test_if_locked
   * @return
   */
  int open(const char *name, int mode, uint test_if_locked) override;

  /**
   * Close array
   * @return
   */
  int close(void) override;

  /**
   * Initialize table scanning
   * @return
   */
  int init_scan(THD *thd, std::unique_ptr<void, decltype(&std::free)> subarray);

  /* Table Scanning */
  int rnd_init(bool scan) override;

  /**
   * Read next Row
   * @return
   */
  int rnd_row(TABLE *table);

  /**
   * Read next row
   * @param buf
   * @return
   */
  int rnd_next(uchar *buf) override;

  /**
   * End read
   * @return
   */
  int rnd_end() override;

  /**
   * Read position based on cordinates stored in pos buffer
   * @param buf ignored
   * @param pos coordinate
   * @return
   */
  int rnd_pos(uchar *buf, uchar *pos) override;

  /**
   * Get current record coordinates and save to allow for later lookup
   * @param record
   */
  void position(const uchar *record) override;

  /**
   * Write row
   * @param buf
   * @return
   */
  int write_row(const uchar *buf) override;

  /**
   *
   * @param rows
   * @param flags
   */
  void start_bulk_insert(ha_rows rows, uint flags) override;

  /**
   *
   * @return
   */
  int end_bulk_insert() override;

  /**
   * flush_write
   * @return
   */
  int flush_write();

  /**
   * Convert a mysql row to attribute/coordinate buffers (columns)
   * @param buf
   * @return
   */
  int mysql_row_to_tiledb_buffers(const uchar *buf);

  /**
   *  Handle condition pushdown of sub conditions
   * @param cond_item
   * @return
   */
  const COND *cond_push_cond(Item_cond *cond_item);

  /**
   *  Handle function condition pushdowns
   * @param func_item
   * @return
   */
  const COND *cond_push_func(const Item_func *func_item);

  /**
  Push condition down to the table handler.

  @param  cond   Condition to be pushed. The condition tree must not be
                 modified by the by the caller.

  @return
    The 'remainder' condition that caller must use to filter out records.
    NULL means the handler will not return rows that do not match the
    passed condition.

  @note
  The pushed conditions form a stack (from which one can remove the
  last pushed condition using cond_pop).
  The table handler filters out rows using (pushed_cond1 AND pushed_cond2
  AND ... AND pushed_condN)
  or less restrictive condition, depending on handler's capabilities.

  handler->ha_reset() call empties the condition stack.
  Calls to rnd_init/rnd_end, index_init/index_end etc do not affect the
  condition stack.
*/
  const COND *cond_push(const COND *cond) override;

  /**
   * Handle the actual pushdown, this is a common function for cond_push and
   * indx_cond_push
   *
   * @param cond condition being pushed
   * @return condition stack which could not be pushed
   */
  const COND *cond_push_local(const COND *cond);

  /**
    Pop the top condition from the condition stack of the storage engine
    for each partition.
  */
  void cond_pop() override;

  /**
   *
   * @param idx
   * @param part
   * @param all_parts
   * @return
   */
  ulong index_flags(uint idx, uint part, bool all_parts) const override;

  /**
   * Returns limit on the number of keys imposed.
   * @return
   */
  uint max_supported_keys() const override { return MAX_INDEXES; }

  /**
   * Mytile doesn't need locks, so we just ignore the store_lock request by
   * returning the original lock data
   * @param thd
   * @param to
   * @param lock_type
   * @return the original lock, to
   */
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type) override;

  /*
   * External lock for table locking
   */
  int external_lock(THD *thd, int lock_type) override;

  /**
   * Helper function to allocate all buffers
   */
  void alloc_buffers(uint64_t size);

  /**
   * Helper function to alloc and set read buffers
   * @param size
   */
  void alloc_read_buffers(uint64_t size);

  /**
   * Helper to free buffers
   */
  void dealloc_buffers();

  /**
   *
   * Converts a tiledb record to mysql buffer using mysql fields
   * @param record_position
   * @param dimensions_only
   * @param TABLE
   * @return status
   */
  int tileToFields(uint64_t record_position, bool dimensions_only,
                   TABLE *table);

  /**
   * Table info
   * @return
   */
  int info(uint) override;

  /**
   * Index read will optional push a key down to tiledb if no existing pushdown
   * has already happened
   *
   * This will then initiate a table scan
   *
   * @param buf unused mysql buffer
   * @param key key to pushdown
   * @param key_len length of key
   * @param find_flag operation (lt, le, eq, ge, gt)
   * @return status
   */
  int index_read(uchar *buf, const uchar *key, uint key_len,
                 enum ha_rkey_function find_flag) override;

  /**
   * Fetch a single row based on a single key
   *
   * This will then initiate a table scan
   *
   * @param buf unused mysql buffer
   * @param idx index number (mytile only support 1 primary key currently)
   * @param key key to pushdown
   * @param keypart_map bitmap of parts of key which are included
   * @param find_flag operation (lt, le, eq, ge, gt)
   * @return status
   */
  int index_read_idx_map(uchar *buf, uint idx, const uchar *key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag) override;

  /**
   * Is the primary key clustered
   * @return false to workaround mariadb assuming sequential index scans are
   * faster than pushdown
   */
  bool primary_key_is_clustered() override { return FALSE; }

  /**
   * Pushdown an index condition
   * @param keyno key number
   * @param idx_cond Condition
   * @return Left over conditions not pushdown
   */
  Item *idx_cond_push(uint keyno, Item *idx_cond) override;

  /**
   * Prepare for index usage, treated here similar to rnd_init
   * @param idx key number to use
   * @param sorted unused
   * @return
   */
  int index_init(uint idx, bool sorted) override;

  /**
   * Treated like rnd_end
   * @return
   */
  int index_end() override;

  /**
   * Read "first" row
   * @param buf
   * @return
   */
  int index_first(uchar *buf) override;

  /**
   * Read next row
   * @param buf
   * @return
   */
  int index_next(uchar *buf) override;

  /**
   * Implement initial records in range
   * Currently returns static large value
   */
  ha_rows records_in_range(uint inx, key_range *min_key,
                           key_range *max_key) override;

private:
  // Table uri
  std::string uri;

  // TileDB context
  tiledb::Context ctx;

  // TileDB Config
  tiledb::Config config;

  // TileDB Array
  std::shared_ptr<tiledb::Array> array;

  // TileDB Query
  std::shared_ptr<tiledb::Query> query;

  // Current record row
  uint64_t record_index = 0;

  // Vector of buffers in field index order
  std::vector<std::shared_ptr<buffer>> buffers;
  std::shared_ptr<buffer> coord_buffer;

  // Number of dimensions, this is used frequently so let's cache it
  uint64_t ndim = 0;

  // Array Schema
  std::unique_ptr<tiledb::ArraySchema> array_schema;

  // Upper bound for number of records so we know stopping condition
  uint64_t total_num_records_UB = 0;

  int64_t records = -2;
  uint64_t records_read = 0;
  tiledb::Query::Status status = tiledb::Query::Status::UNINITIALIZED;

  // Vector of pushdowns
  std::vector<std::vector<std::shared_ptr<tile::range>>> pushdown_ranges;

  // Vector of pushdown in ranges
  std::vector<std::vector<std::shared_ptr<tile::range>>> pushdown_in_ranges;

  // read buffer size
  uint64_t read_buffer_size = 0;

  // write buffer size
  uint64_t write_buffer_size = 0;

  // in bulk write mode
  bool bulk_write = false;

  // Upper bound for records, used for table stats by optimized
  uint64_t records_upper_bound = 2;

  /**
   * Helper to setup writes
   */
  void setup_write();

  /**
   * Helper to end and finalize writes
   * @return
   */
  int finalize_write();

  /**
   * Helper function which validates the array is open for reads
   */
  void open_array_for_reads(THD *thd);

  /**
   * Helper function which validates the array is open for writes
   */
  void open_array_for_writes(THD *thd);

  /**
   * Checks if there are any ranges pushed
   * @return
   */
  bool valid_pushed_ranges();

  /**
   * Checks if there are any in ranges pushed
   * @return
   */
  bool valid_pushed_in_ranges();

  /**
   * Reset condition pushdowns to be for key conditions
   * @param key key(s) to pushdown
   * @param key_len
   * @param find_flag equality condition of last key
   * @return
   */
  int reset_pushdowns_for_key(const uchar *key, uint key_len,
                              enum ha_rkey_function find_flag);
};
} // namespace tile
