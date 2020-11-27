/*
** Licensed under the GNU Lesser General Public License v3 or later
*/
#pragma once

#define MYSQL_SERVER 1

#include "my_global.h" /* ulonglong */
#include "mytile-errors.h"
#include "mytile-buffer.h"
#include <field.h>
#include <mysqld_error.h> /* ER_UNKNOWN_ERROR */
#include <tiledb/tiledb>

/** Table options */
struct ha_table_option_struct {
  const char *array_uri;
  ulonglong capacity;
  uint array_type;
  ulonglong cell_order;
  ulonglong tile_order;
  ulonglong open_at;
  const char *encryption_key;
  const char *coordinate_filters;
  const char *offset_filters;
};

/** Field options */
struct ha_field_option_struct {
  bool dimension;
  const char *lower_bound;
  const char *upper_bound;
  const char *tile_extent;
  const char *filters;
};

namespace tile {
typedef struct ::ha_table_option_struct ha_table_option_struct;
typedef struct ::ha_field_option_struct ha_field_option_struct;

/**
 * Converts a mysql type to a tiledb_datatype_t
 * @param type
 * @param signedInt
 * @return
 */
tiledb_datatype_t mysqlTypeToTileDBType(int type, bool signedInt);

/**
 * Converts a tiledb_datatype_t to a mysql type
 * @param type
 * @return
 */
int TileDBTypeToMysqlType(tiledb_datatype_t type, bool multi_value);

/**
 * Converts a value of tiledb_datatype_t to a string
 * @param type
 * @param value
 */
std::string TileDBTypeValueToString(tiledb_datatype_t type, const void *value,
                                    uint64_t value_size);

/**
 * Create the text string for a mysql type
 * e.g. MYSQL_TYPE_TINY -> "TINY"
 * @param type
 * @return
 */
std::string MysqlTypeString(int type);

/**
 * Returns if a tiledb datatype is unsigned or not
 * @param type
 * @return
 */
bool TileDBTypeIsUnsigned(tiledb_datatype_t type);

/**
 * Returns if a tiedb datatype is a date/time type
 * @param type
 * @return
 */
bool TileDBDateTimeType(tiledb_datatype_t type);

/**
 * Returns if a mysql type is a blob type
 * @param type
 * @return
 */
bool MysqlBlobType(enum_field_types type);

/**
 * Returns if a mysql type is a datetime type
 * @param type
 * @return
 */
bool MysqlDatetimeType(enum_field_types type);

/**
 * Convert a MYSQL_TIME to a TileDB int64 time value
 * @param mysql_time
 * @return
 */
int64_t MysqlTimeToTileDBTimeVal(THD *thd, const MYSQL_TIME &mysql_time,
                                 tiledb_datatype_t datatype);

/**
 * Used when create table is being translated to create array
 * @param ctx
 * @param field
 * @param filterList
 * @return tiledb::Attribute from field
 */
tiledb::Attribute create_field_attribute(tiledb::Context &ctx, Field *field,
                                         const tiledb::FilterList &filterList);

/**
 *
 * Used when create table is being translated to create array
 * @param ctx
 * @param field
 * @return tiledb::Dimension from field
 */
tiledb::Dimension create_field_dimension(tiledb::Context &ctx, Field *field);

// -- helpers --

/**
 * parse_value
 * @tparam T
 * @param s
 * @return
 */
template <typename T> T parse_value(const std::string &s) {
  T result;
  std::istringstream ss(s);
  if (typeid(T) == typeid(int8_t) || typeid(T) == typeid(uint8_t)) {
    int32_t tmp = 0;
    ss >> tmp;
    result = static_cast<T>(tmp);
  } else {
    ss >> result;
  }
  if (ss.fail())
    throw std::invalid_argument("Cannot parse value from '" + s + "'");
  return result;
}

/**
 * get dim domain
 * @tparam T
 * @param field
 * @return
 */
template <typename T> std::array<T, 2> get_dim_domain(Field *field) {
  std::array<T, 2> domain = {std::numeric_limits<T>::lowest(),
                             std::numeric_limits<T>::max()};
  domain[1] -= parse_value<T>(field->option_struct->tile_extent);
  if (field->option_struct->lower_bound != nullptr)
    domain[0] = parse_value<T>(field->option_struct->lower_bound);
  if (field->option_struct->upper_bound != nullptr)
    domain[1] = parse_value<T>(field->option_struct->upper_bound);
  return domain;
}

/**
 * create dim
 * @tparam T
 * @param ctx
 * @param field
 * @param datatype
 * @return
 */
template <typename T>
tiledb::Dimension create_dim(tiledb::Context &ctx, Field *field,
                             tiledb_datatype_t datatype) {
  if (field->option_struct->tile_extent == nullptr) {
    my_printf_error(ER_UNKNOWN_ERROR,
                    "Invalid dimension, must specify tile extent",
                    ME_ERROR_LOG | ME_FATAL);
    throw std::invalid_argument("Must specify tile extent");
  }

  std::array<T, 2> domain = get_dim_domain<T>(field);
  T tile_extent = parse_value<T>(field->option_struct->tile_extent);
  return tiledb::Dimension::create(ctx, field->field_name.str, datatype,
                                   domain.data(), &tile_extent);
}

/**
 * alloc buffer
 * @param type
 * @param size
 * @return
 */
void *alloc_buffer(tiledb_datatype_t type, uint64_t size);

/**
 * alloc buffer
 * @tparam T
 * @param size
 * @return
 */
template <typename T> T *alloc_buffer(uint64_t size) {
  return static_cast<T *>(malloc(size));
}

/**
 * set field nullable from validity buffer
 * @param buff
 * @param field
 * @param i
 * @return true if null set
 */
bool set_field_null_from_validity(std::shared_ptr<buffer> &buff,
                                  Field *field,
                                  uint64_t i);

/**
 * set field from datetime type
 * @param thd
 * @param field
 * @param buff
 * @param i
 * @param seconds
 * @param second_part
 * @param type
 * @return
 */
int set_datetime_field(THD *thd, Field *field, std::shared_ptr<buffer> &buff,
                       uint64_t i, uint64_t seconds, uint64_t second_part,
                       enum_mysql_timestamp_type type);

/**
 * Set field from type
 * @param thd
 * @param field
 * @param buff
 * @param i
 * @return
 */
int set_field(THD *thd, Field *field, std::shared_ptr<buffer> &buff,
              uint64_t i);

/**
 * Set field from var string
 * @tparam T
 * @param field
 * @param buff
 * @param i
 * @param charset_info
 */
template <typename T>
int set_var_string_field(Field *field, std::shared_ptr<buffer> &buff,
                     uint64_t i, charset_info_st *charset_info) {

  const uint64_t *offset_buffer = buff->offset_buffer;
  uint64_t offset_buffer_size = buff->offset_buffer_size;
  T *buffer = static_cast<T *>(buff->buffer);
  uint64_t buffer_size = buff->buffer_size;

  uint64_t end_position = i + 1;
  uint64_t start_position = 0;
  // If its not the first value, we need to see where the previous position
  // ended to know where to start.
  if (i > 0) {
    start_position = offset_buffer[i];
  }

  // If the first byte is marked invalid, this is a null field
  if (set_field_null_from_validity(buff, field, start_position)) {
    return 0;
  }

  // If the current position is equal to the number of results - 1 then we are
  // at the last varchar value
  if (i >= (offset_buffer_size / sizeof(uint64_t)) - 1) {
    end_position = buffer_size / sizeof(T);
  } else { // Else read the end from the next offset.
    end_position = offset_buffer[i + 1];
  }
  size_t size = end_position - start_position;
  return field->store(reinterpret_cast<char *>(&buffer[start_position]), size,
                      charset_info);
}

/**
 * Set field from fixed size (1) varchar
 * @tparam T
 * @param field
 * @param buff
 * @param i
 * @param charset_info
 */
template <typename T>
int set_fixed_string_field(Field *field, std::shared_ptr<buffer> &buff,
                     uint64_t i, charset_info_st *charset_info) {
  if (set_field_null_from_validity(buff, field, i)) {
    return 0;
  }

  T* buffer = static_cast<T *>(buff->buffer);
  uint64_t fixed_size_elements = buff->fixed_size_elements;

  return field->store(reinterpret_cast<char *>(&buffer[i]), fixed_size_elements,
                      charset_info);
}

/**
 * Set field from string
 * @tparam T
 * @param field
 * @param buff
 * @param i
 * @param charset_info
 */
template <typename T>
int set_string_field(Field *field, std::shared_ptr<buffer> &buff, uint64_t i,
                     charset_info_st *charset_info) {
  if (buff->offset_buffer == nullptr) {
    return set_fixed_string_field<T>(field, buff, i, charset_info);
  }
  return set_var_string_field<T>(field, buff, i, charset_info);
}

/**
 * Set field from type
 * @tparam T
 * @param field
 * @param i
 * @param buff
 * @return
 */
template <typename T> int set_field(Field *field, uint64_t i,
                                    std::shared_ptr<buffer> &buff) {
  if (set_field_null_from_validity(buff, field, i)) {
    return 0;
  }

  void *buffer = buff->buffer;
  T val = static_cast<T *>(buffer)[i];
  if (std::is_floating_point<T>())
    return field->store(val);
  return field->store(val, std::is_signed<T>());
}

/**
 * Set field from type
 * @tparam T
 * @param field
 * @param buff
 * @param i
 * @return
 */
template <typename T>
int set_field(Field *field, std::shared_ptr<buffer> &buff, uint64_t i) {
  return set_field<T>(field, i, buff);
}

/**
 * Set string buffer from field
 * @tparam T
 * @param field
 * @param field_null
 * @param buff
 * @param i
 * @return
 */
template <typename T>
int set_string_buffer_from_field(Field *field,
                                 bool field_null,
                                 std::shared_ptr<buffer> &buff,
                                 uint64_t i) {

  // Validate we are not over the offset size
  if ((i * sizeof(uint64_t)) > buff->allocated_offset_buffer_size) {
    return ERR_WRITE_FLUSH_NEEDED;
  }

  char strbuff[MAX_FIELD_WIDTH];
  String str(strbuff, sizeof(strbuff), field->charset()), *res;

  res = field->val_str(&str);

  // Find start position to copy buffer to
  uint64_t start = 0;
  if (i > 0) {
    start = buff->buffer_size / sizeof(T);
  }

  // Validate there is enough space on the buffer to copy the field into
  if ((start + res->length()) * sizeof(T) > buff->allocated_buffer_size) {
    return ERR_WRITE_FLUSH_NEEDED;
  }

  // Copy string
  memcpy(static_cast<T *>(buff->buffer) + start, res->ptr(), res->length());

  buff->buffer_size += res->length() * sizeof(T);
  buff->offset_buffer_size += sizeof(uint64_t);
  buff->offset_buffer[i] = start;

  if (buff->validity_buffer != nullptr) {
    if (field_null) {
      // XXX : zero length single cell writes are not supported (write some trash)
      if (res->length() == 0) {
        memcpy(static_cast<T *>(buff->buffer) + start, "0", 1);
        buff->buffer_size += 1;
        memset(buff->validity_buffer + start, (uint8_t)0, 1);
      } else {
        memset(buff->validity_buffer + start, (uint8_t)0, res->length());
      }
    } else {
      memset(buff->validity_buffer + start, (uint8_t)1, res->length());
    }
  }

  return 0;
}

/**
 * Set fixed string buffer from field
 * @tparam T
 * @param field
 * @param field_null
 * @param buff
 * @param i
 * @return
 */
template <typename T>
int set_fixed_string_buffer_from_field(Field *field,
                                       bool field_null,
                                       std::shared_ptr<buffer> &buff,
                                       uint64_t i) {

  // Validate there is enough space on the buffer to copy the field into
  if ((((i * buff->fixed_size_elements) + buff->buffer_offset) * sizeof(T)) >
      buff->allocated_buffer_size) {
    return ERR_WRITE_FLUSH_NEEDED;
  }

  char strbuff[MAX_FIELD_WIDTH];
  String str(strbuff, sizeof(strbuff), field->charset()), *res;

  res = field->val_str(&str);

  // Find start position to copy buffer to
  uint64_t start = i;
  if (buff->fixed_size_elements > 1) {
    start *= buff->fixed_size_elements;
  }

  // Copy string
  memcpy(static_cast<T *>(buff->buffer) + start, res->ptr(),
         buff->fixed_size_elements);

  buff->buffer_size += buff->fixed_size_elements * sizeof(char);

  if (buff->validity_buffer != nullptr) {
    if (field_null) {
      memset(buff->validity_buffer + start, (uint8_t)0, buff->fixed_size_elements);
    } else {
      memset(buff->validity_buffer + start, (uint8_t)1, buff->fixed_size_elements);
    }
  }

  return 0;
}

/**
 * Set buffer from field
 * @tparam T
 * @param val
 * @param field_null
 * @param buff
 * @param i
 * @return
 */
template <typename T>
int set_buffer_from_field(T val, bool field_null, std::shared_ptr<buffer> &buff, uint64_t i) {

  // Validate there is enough space on the buffer to copy the field into
  if ((((i * buff->fixed_size_elements) + buff->buffer_offset) * sizeof(T)) >
      buff->allocated_buffer_size) {
    return ERR_WRITE_FLUSH_NEEDED;
  }

  static_cast<T *>(
      buff->buffer)[(i * buff->fixed_size_elements) + buff->buffer_offset] =
      val;
  buff->buffer_size += sizeof(T);

  if (buff->validity_buffer != nullptr) {
    if (field_null) {
      buff->validity_buffer[i] = 0;
    } else {
      buff->validity_buffer[i] = 1;
    }
  }

  return 0;
}

/**
 * Set buffer from field
 * @param field
 * @param buff
 * @param i
 * @param thd
 * @param check_null
 * @return
 */
int set_buffer_from_field(Field *field, std::shared_ptr<buffer> &buff,
                          uint64_t i, THD *thd, bool check_null);

/**
 * parse filter list
 * @param ctx
 * @param filter_csv
 * @return
 */
tiledb::FilterList parse_filter_list(tiledb::Context &ctx,
                                     const char *filter_csv);

/**
 * filter list to string
 * @param filter_list
 * @return
 */
std::string filter_list_to_str(const tiledb::FilterList &filter_list);

// -- end helpers --

/**
 *  Function to get list of default fill in values from TileDB
 *
 * @param type datatype of fill value
 * @return pointer to default fill value
 */
const void *default_tiledb_fill_value(const tiledb_datatype_t &type);

/**
 * Checks if a fill value is default
 * @param type datatype of value
 * @param value pointer to value
 * @return true if default tiledb value (not user custom value)
 */
bool is_fill_value_default(const tiledb_datatype_t &type, const void *value,
                           const uint64_t &size);

/**
 * Check if a datatype is a string type or not
 * @param type datatype
 * @return true if is a string datatype
 */
bool is_string_datatype(const tiledb_datatype_t &type);

namespace constants {
/** The special value for an empty int32. */
extern const int empty_int32;

/** The special value for an empty int64. */
extern const int64_t empty_int64;

/** The special value for an empty float32. */
extern const float empty_float32;

/** The special value for an empty float64. */
extern const double empty_float64;

/** The special value for an empty char. */
extern const char empty_char;

/** The special value for an empty int8. */
extern const int8_t empty_int8;

/** The special value for an empty uint8. */
extern const uint8_t empty_uint8;

/** The special value for an empty int16. */
extern const int16_t empty_int16;

/** The special value for an empty uint16. */
extern const uint16_t empty_uint16;

/** The special value for an empty uint32. */
extern const uint32_t empty_uint32;

/** The special value for an empty uint64. */
extern const uint64_t empty_uint64;

/** The special value for an empty ASCII. */
extern const uint8_t empty_ascii;

/** The special value for an empty UTF8. */
extern const uint8_t empty_utf8;

/** The special value for an empty UTF16. */
extern const uint16_t empty_utf16;

/** The special value for an empty UTF32. */
extern const uint32_t empty_utf32;

/** The special value for an empty UCS2. */
extern const uint16_t empty_ucs2;

/** The special value for an empty UCS4. */
extern const uint32_t empty_ucs4;

/** The special value for an empty ANY. */
extern const uint8_t empty_any;
} // namespace constants

} // namespace tile
