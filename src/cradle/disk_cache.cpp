#include <cradle/disk_cache.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include <sqlite3.h>

#include <hashids.h>

#include <cradle/fs/app_dirs.hpp>

namespace cradle {

struct disk_cache_impl
{
    file_path dir;

    sqlite3* db = nullptr;

    // prepared statements
    sqlite3_stmt* database_version_query = nullptr;
    sqlite3_stmt* record_usage_statement = nullptr;
    sqlite3_stmt* update_entry_value_statement = nullptr;
    sqlite3_stmt* insert_new_value_statement = nullptr;
    sqlite3_stmt* initiate_insert_statement = nullptr;
    sqlite3_stmt* finish_insert_statement = nullptr;
    sqlite3_stmt* remove_entry_statement = nullptr;
    sqlite3_stmt* look_up_entry_query = nullptr;
    sqlite3_stmt* cache_size_query = nullptr;
    sqlite3_stmt* entry_count_query = nullptr;
    sqlite3_stmt* entry_list_query = nullptr;
    sqlite3_stmt* lru_entry_list_query = nullptr;

    int64_t size_limit;

    // used to track when we need to check if the cache is too big
    int64_t bytes_inserted_since_last_sweep = 0;

    // list of IDs that whose usage needs to be recorded
    std::vector<int64_t> usage_record_buffer;

    boost::posix_time::ptime latest_activity;

    // protects all access to the cache
    boost::mutex mutex;
};

// SQLITE UTILITIES

void static
open_db(sqlite3** db, file_path const& file)
{
    if (sqlite3_open(file.string().c_str(), db) != SQLITE_OK)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(file.parent_path()) <<
                internal_error_message_info("failed to create disk cache index file (index.db)"));
    }
}

void static
throw_query_error(disk_cache_impl const& cache, string const& sql, string const& error)
{
    CRADLE_THROW(
        disk_cache_failure() <<
            disk_cache_path_info(cache.dir) <<
            internal_error_message_info(
                "error executing SQL query in index.db\n"
                "SQL query: " + sql + "\n" +
                "error: " + error));
}

string static
copy_and_free_message(char* msg)
{
    if (msg)
    {
        string s = msg;
        sqlite3_free(msg);
        return s;
    }
    else
        return "";
}

void static
execute_sql(disk_cache_impl const& cache, string const& sql)
{
    char *msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), 0, 0, &msg);
    string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
        throw_query_error(cache, sql, error);
}

// Check a return code from SQLite.
void static
check_sqlite_code(disk_cache_impl const& cache, int code)
{
    if (code != SQLITE_OK)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info(
                    string("SQLite error: ") + sqlite3_errstr(code)));
    }
}

// Create a prepared statement.
// This checks to make sure that the creation was successful, so the returned
// pointer is always valid.
static sqlite3_stmt*
prepare_statement(disk_cache_impl const& cache, string const& sql)
{
    sqlite3_stmt* statement;
    auto code =
        sqlite3_prepare_v2(
            cache.db,
            sql.c_str(), boost::numeric_cast<int>(sql.length()),
            &statement,
            nullptr);
    if (code != SQLITE_OK)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info(
                    "error preparing SQL query\n"
                    "SQL query: " + sql + "\n"
                    "error: " + sqlite3_errstr(code)));
    }
    return statement;
}

// Bind a 32-bit integer to a parameter of a prepared statement.
void static
bind_int32(
    disk_cache_impl const& cache,
    sqlite3_stmt* statement,
    int parameter_index,
    int value)
{
    check_sqlite_code(
        cache,
        sqlite3_bind_int(statement, parameter_index, value));
}

// Bind a 64-bit integer to a parameter of a prepared statement.
void static
bind_int64(
    disk_cache_impl const& cache,
    sqlite3_stmt* statement,
    int parameter_index,
    int64_t value)
{
    check_sqlite_code(
        cache,
        sqlite3_bind_int64(statement, parameter_index, value));
}

// Bind a string to a parameter of a prepared statement.
void static
bind_string(
    disk_cache_impl const& cache,
    sqlite3_stmt* statement,
    int parameter_index,
    string const& value)
{
    check_sqlite_code(
        cache,
        sqlite3_bind_text64(
            statement,
            parameter_index,
            value.c_str(),
            value.size(),
            SQLITE_STATIC,
            SQLITE_UTF8));
}

// Bind a blob to a parameter of a prepared statement.
void static
bind_blob(
    disk_cache_impl const& cache,
    sqlite3_stmt* statement,
    int parameter_index,
    string const& value)
{
    check_sqlite_code(
        cache,
        sqlite3_bind_blob64(
            statement,
            parameter_index,
            value.c_str(),
            value.size(),
            SQLITE_STATIC));
}

// Execute a prepared statement (with variables already bound to it) and check
// that it finished successfully.
// This should only be used for statements that don't return results.
void static
execute_prepared_statement(disk_cache_impl const& cache, sqlite3_stmt* statement)
{
    auto code = sqlite3_step(statement);
    if (code != SQLITE_DONE)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info(
                    string("SQL query failed\n") +
                    "error: " + sqlite3_errstr(code)));
    }
    check_sqlite_code(cache, sqlite3_reset(statement));
}

struct sqlite_row
{
    sqlite3_stmt* statement;
};

bool static
has_value(sqlite_row& row, int column_index)
{
    return sqlite3_column_type(row.statement, column_index) != SQLITE_NULL;
}

bool static
read_bool(sqlite_row& row, int column_index)
{
    return sqlite3_column_int(row.statement, column_index) != 0;
}

int static
read_int32(sqlite_row& row, int column_index)
{
    return sqlite3_column_int(row.statement, column_index);
}

int64_t static
read_int64(sqlite_row& row, int column_index)
{
    return sqlite3_column_int64(row.statement, column_index);
}

string static
read_string(sqlite_row& row, int column_index)
{
    return
        reinterpret_cast<char const*>(
            sqlite3_column_text(row.statement, column_index));
}

string static
read_blob(sqlite_row& row, int column_index)
{
    return
        reinterpret_cast<char const*>(
            sqlite3_column_blob(row.statement, column_index));
}

// Execute a prepared statement (with variables already bound to it), pass all
// the rows from the result set into the supplied callback, and check that the
// query finishes successfully.
struct expected_column_count
{
    int value;
};
struct single_row_result
{
    bool value;
};
template<class RowHandler>
void static
execute_prepared_statement(
    disk_cache_impl const& cache,
    sqlite3_stmt* statement,
    expected_column_count expected_columns,
    single_row_result single_row,
    RowHandler const& row_handler)
{
    int row_count = 0;
    int code;
    while (true)
    {
        code = sqlite3_step(statement);
        if (code == SQLITE_ROW)
        {
            if (sqlite3_column_count(statement) != expected_columns.value)
            {
                CRADLE_THROW(
                    disk_cache_failure() <<
                        disk_cache_path_info(cache.dir) <<
                        internal_error_message_info(
                            string("SQL query result column count incorrect\n")));
            }
            sqlite_row row;
            row.statement = statement;
            row_handler(row);
            ++row_count;
        }
        else
        {
            break;
        }
    }
    if (code != SQLITE_DONE)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info(
                    string("SQL query failed\n") +
                    "error: " + sqlite3_errstr(code)));
    }
    if (single_row.value && row_count != 1)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info(
                    string("SQL query row count incorrect\n")));
    }
    check_sqlite_code(cache, sqlite3_reset(statement));
}

// QUERIES

// Get the total size of all entries in the cache.
int64_t static
get_cache_size(disk_cache_impl& cache)
{
    int64_t size;
    execute_prepared_statement(
        cache,
        cache.cache_size_query,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row)
        {
            size = read_int64(row, 0);
        });
    return size;
}

// Get the total number of valid entries in the cache.
int64_t static
get_cache_entry_count(disk_cache_impl& cache)
{
    int64_t count;
    execute_prepared_statement(
        cache,
        cache.entry_count_query,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row)
        {
            count = read_int64(row, 0);
        });
    return count;
}

// Get a list of entries in the cache.
std::vector<disk_cache_entry> static
get_entry_list(disk_cache_impl& cache)
{
    std::vector<disk_cache_entry> entries;
    execute_prepared_statement(
        cache,
        cache.entry_list_query,
        expected_column_count{5},
        single_row_result{false},
        [&](sqlite_row& row)
        {
            disk_cache_entry e;
            e.key = read_string(row, 0);
            e.id = read_int64(row, 1);
            e.in_db = read_int64(row, 2) && read_bool(row, 2);
            e.size = has_value(row, 3) ? read_int64(row, 3) : 0;
            e.crc32 = has_value(row, 4) ? read_int32(row, 4) : 0;
            entries.push_back(e);
        });
    return entries;
}

// Get a list of entries in the cache in LRU order.
struct lru_entry
{
    int64_t id, size;
    bool in_db;
};
typedef std::vector<lru_entry> lru_entry_list;
lru_entry_list static
get_lru_entries(disk_cache_impl& cache)
{
    lru_entry_list entries;
    execute_prepared_statement(
        cache,
        cache.lru_entry_list_query,
        expected_column_count{3},
        single_row_result{false},
        [&](sqlite_row& row)
        {
            lru_entry e;
            e.id = read_int64(row, 0);
            e.size = has_value(row, 1) ? read_int64(row, 1) : 0;
            e.in_db = has_value(row, 2) && read_bool(row, 2);
            entries.push_back(e);
        });
    return entries;
}

// Get the entry associated with a particular key (if any).
optional<disk_cache_entry> static
look_up(
    disk_cache_impl const& cache,
    string const& key,
    bool only_if_valid)
{
    bool exists = false;
    int64_t id;
    bool valid;
    bool in_db;
    optional<string> value;
    int64_t size;
    uint32_t crc32;

    bind_string(cache, cache.look_up_entry_query, 1, key);
    execute_prepared_statement(
        cache,
        cache.look_up_entry_query,
        expected_column_count{6},
        single_row_result{false},
        [&](sqlite_row& row)
        {
            id = read_int64(row, 0);
            valid = read_bool(row, 1);
            in_db = has_value(row, 2) && read_bool(row, 2);
            value = has_value(row, 3) ? some(read_string(row, 3)) : none;
            size = has_value(row, 4) ? read_int64(row, 4) : 0;
            crc32 = has_value(row, 5) ? read_int32(row, 5) : 0;
            exists = true;
        });

    return
        (exists && (!only_if_valid || valid))
      ? some(make_disk_cache_entry(key, id, in_db, value, size, crc32))
      : none;
}

// OTHER UTILITIES

file_path static
get_path_for_id(disk_cache_impl& cache, int64_t id)
{
    hashidsxx::Hashids hash("cradle", 6);
    return cache.dir / hash.encode(&id, &id + 1);
}

void static
remove_entry(disk_cache_impl& cache, int64_t id, bool remove_file = true)
{
    file_path path = get_path_for_id(cache, id);
    if (remove_file && exists(path))
        remove(path);

    bind_int64(cache, cache.remove_entry_statement, 1, id);
    execute_prepared_statement(
        cache,
        cache.remove_entry_statement);
}

void static
enforce_cache_size_limit(disk_cache_impl& cache)
{
    try
    {
        int64_t size = get_cache_size(cache);
        if (size > cache.size_limit)
        {
            auto lru_entries = get_lru_entries(cache);
            std::vector<lru_entry>::const_iterator
                i = lru_entries.begin(),
                end_i = lru_entries.end();
            while (size > cache.size_limit && i != end_i)
            {
                try
                {
                    remove_entry(cache, i->id, !i->in_db);
                    size -= i->size;
                    ++i;
                }
                catch (...)
                {
                }
            }
        }
        cache.bytes_inserted_since_last_sweep = 0;
    }
    catch (...)
    {
    }
}

void static
record_activity(disk_cache_impl& cache)
{
    cache.latest_activity = boost::posix_time::microsec_clock::local_time();
}

void static
record_usage_to_db(disk_cache_impl const& cache, int64_t id)
{
    bind_int64(cache, cache.record_usage_statement, 1, id);
    execute_prepared_statement(
        cache,
        cache.record_usage_statement);
}

void static
write_usage_records(disk_cache_impl& cache)
{
    for (auto const& record : cache.usage_record_buffer)
        record_usage_to_db(cache, record);
    cache.usage_record_buffer.clear();
}

void
record_cache_growth(disk_cache_impl& cache, size_t size)
{
    cache.bytes_inserted_since_last_sweep += size;
    // Allow the cache to write out roughly 1% of its capacity between size checks.
    // (So it could exceed its limit slightly, but only temporarily, and not by much.)
    if (cache.bytes_inserted_since_last_sweep > cache.size_limit / 0x80)
        enforce_cache_size_limit(cache);
}

void static
shut_down(disk_cache_impl& cache)
{
    if (cache.db)
    {
        sqlite3_finalize(cache.database_version_query);
        sqlite3_finalize(cache.record_usage_statement);
        sqlite3_finalize(cache.update_entry_value_statement);
        sqlite3_finalize(cache.insert_new_value_statement);
        sqlite3_finalize(cache.initiate_insert_statement);
        sqlite3_finalize(cache.finish_insert_statement);
        sqlite3_finalize(cache.remove_entry_statement);
        sqlite3_finalize(cache.look_up_entry_query);
        sqlite3_finalize(cache.cache_size_query);
        sqlite3_finalize(cache.entry_count_query);
        sqlite3_finalize(cache.entry_list_query);
        sqlite3_finalize(cache.lru_entry_list_query);
        sqlite3_close(cache.db);
        cache.db = nullptr;
    }
}

// Open (or create) the database file and verify that the version number is
// what we expect.
void static
open_and_check_db(disk_cache_impl& cache)
{
    int const expected_database_version = 1;

    open_db(&cache.db, cache.dir / "index.db");

    // Get the version number embedded in the database.
    cache.database_version_query = prepare_statement(cache, "pragma user_version;");
    int database_version;
    execute_prepared_statement(
        cache,
        cache.database_version_query,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row)
        {
            database_version = read_int32(row, 0);
        });

    // A database_version of 0 indicates a fresh database, so initialize it.
    if (database_version == 0)
    {
        execute_sql(cache,
            "create table entries("
            " id integer primary key,"
            " key text unique not null,"
            " valid boolean not null,"
            " last_accessed datetime,"
            " in_db boolean,"
            " value blob,"
            " size integer,"
            " crc32 integer);");
        execute_sql(cache,
            "pragma user_version = " + lexical_cast<string>(expected_database_version) + ";");
    }
    // If we find a database from a different version, abort.
    else if (database_version != expected_database_version)
    {
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info("incompatible database"));
    }
}

void static
initialize(disk_cache_impl& cache, disk_cache_config const& config)
{
    cache.dir = config.directory ? *config.directory : get_shared_cache_dir(none, "cradle");
    // Create the directory if it doesn't exist.
    if (!exists(cache.dir))
        create_directory(cache.dir);

    cache.size_limit = config.size_limit;

    // Open the database file.
    try
    {
        open_and_check_db(cache);
    }
    catch (...)
    {
        // If the first attempt fails, we may have an incompatible or corrupt
        // database, so shut everything down, clear out the directory, and try
        // again.
        shut_down(cache);
        for (boost::filesystem::directory_iterator end, i(cache.dir); i != end; ++i)
            remove_all(i->path());
        open_and_check_db(cache);
    }

    // Set various performance tuning flags.
    execute_sql(cache,
        "pragma synchronous = off;");
    execute_sql(cache,
        "pragma locking_mode = exclusive;");
    execute_sql(cache,
        "pragma journal_mode = memory;");

    // Initialize our prepared statements.
    cache.record_usage_statement =
        prepare_statement(cache,
            "update entries set last_accessed=strftime('%Y-%m-%d %H:%M:%f', 'now') where id=?1;");
    cache.update_entry_value_statement =
        prepare_statement(cache,
            "update entries set valid=1, in_db=1, size=?1, value=?2,"
            " last_accessed=strftime('%Y-%m-%d %H:%M:%f', 'now')"
            " where id=?3;");
    cache.insert_new_value_statement =
        prepare_statement(cache,
            "insert into entries(key, valid, in_db, size, value, last_accessed)"
            " values(?1, 1, 1, ?2, ?3, strftime('%Y-%m-%d %H:%M:%f', 'now'));");
    cache.initiate_insert_statement =
        prepare_statement(cache,
            "insert into entries(key, valid, in_db) values (?1, 0, 0);");
    cache.finish_insert_statement =
        prepare_statement(cache,
            "update entries set valid=1, in_db=0, size=?1, crc32=?2,"
            " last_accessed=strftime('%Y-%m-%d %H:%M:%f', 'now')"
            " where id=?3;");
    cache.remove_entry_statement =
        prepare_statement(cache,
            "delete from entries where id=?1;");
    cache.look_up_entry_query =
        prepare_statement(cache,
            "select id, valid, in_db, value, size, crc32 from entries where key=?1;");
    cache.cache_size_query =
        prepare_statement(cache,
            "select sum(size) from entries;");
    cache.entry_count_query =
        prepare_statement(cache,
            "select count(id) from entries where valid = 1;");
    cache.entry_list_query =
        prepare_statement(cache,
            "select key, id, in_db, size, crc32 from entries where valid = 1"
            " order by last_accessed;");
    cache.lru_entry_list_query =
        prepare_statement(cache,
            "select id, size, in_db from entries order by valid, last_accessed;");

    // Do initial housekeeping.
    record_activity(cache);
    enforce_cache_size_limit(cache);
}

void static
check_initialization(disk_cache_impl& cache)
{
    if (!cache.db)
    {
        CRADLE_THROW(disk_cache_uninitialized());
    }
}


// API

disk_cache::disk_cache()
{
    this->impl_ = new disk_cache_impl;
}

disk_cache::disk_cache(disk_cache_config const& config)
{
    this->impl_ = new disk_cache_impl;
    this->reset(config);
}

disk_cache::~disk_cache()
{
    {
        auto& cache = *this->impl_;
        boost::lock_guard<boost::mutex> lock(cache.mutex);
        shut_down(*this->impl_);
    }
    delete this->impl_;
}

void
disk_cache::reset(disk_cache_config const& config)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    shut_down(cache);
    initialize(cache, config);
}

// Reset the cache to an uninitialized state.
void
disk_cache::reset()
{
    shut_down(*impl_);
}

bool
disk_cache::is_initialized()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);

    return cache.db != nullptr;
}

disk_cache_info
disk_cache::get_summary_info()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    // Note that these are actually inconsistent since the size includes
    // invalid entries, while the entry count does not, but I think that's
    // reasonable behavior and in any case not a big deal.
    disk_cache_info info;
    info.directory = cache.dir.string<string>();
    info.entry_count = get_cache_entry_count(cache);
    info.total_size = get_cache_size(cache);
    return info;
}

std::vector<disk_cache_entry>
disk_cache::get_entry_list()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    return cradle::get_entry_list(cache);
}

void
disk_cache::remove_entry(int64_t id)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    cradle::remove_entry(cache, id);
}

void
disk_cache::clear()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    for (auto const& entry : get_lru_entries(cache))
    {
        try
        {
            cradle::remove_entry(cache, entry.id, !entry.in_db);
        }
        catch (...)
        {
        }
    }
}

optional<disk_cache_entry>
disk_cache::find(string const& key)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    return look_up(cache, key, true);
}

void
disk_cache::insert(string const& key, string const& value)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    auto entry = look_up(cache, key, false);
    if (entry)
    {
        bind_int64(cache, cache.update_entry_value_statement, 1, value.size());
        bind_blob(cache, cache.update_entry_value_statement, 2, value);
        bind_string(cache, cache.update_entry_value_statement, 3, key);
        execute_prepared_statement(cache, cache.update_entry_value_statement);
    }
    else
    {
        bind_string(cache, cache.insert_new_value_statement, 1, key);
        bind_int64(cache, cache.insert_new_value_statement, 2, value.size());
        bind_blob(cache, cache.insert_new_value_statement, 3, value);
        execute_prepared_statement(cache, cache.insert_new_value_statement);
    }

    record_cache_growth(cache, value.size());
}

int64_t
disk_cache::initiate_insert(string const& key)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    auto entry = look_up(cache, key, false);
    if (entry)
        return entry->id;

    bind_string(cache, cache.initiate_insert_statement, 1, key);
    execute_prepared_statement(cache, cache.initiate_insert_statement);

    // Get the ID that was inserted.
    entry = look_up(cache, key, false);
    if (!entry)
    {
        // Since we checked that the insert succceeded, we really shouldn't
        // get here.
        CRADLE_THROW(
            disk_cache_failure() <<
                disk_cache_path_info(cache.dir) <<
                internal_error_message_info("failed to create entry in index.db"));
    }

    return entry->id;
}

void
disk_cache::finish_insert(int64_t id, uint32_t crc32)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    record_activity(cache);

    int64_t size = file_size(cradle::get_path_for_id(cache, id));

    bind_int64(cache, cache.finish_insert_statement, 1, size);
    bind_int32(cache, cache.finish_insert_statement, 2, crc32);
    bind_int64(cache, cache.finish_insert_statement, 3, id);
    execute_prepared_statement(cache, cache.finish_insert_statement);

    record_cache_growth(cache, size);
}

file_path
disk_cache::get_path_for_id(int64_t id)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    return cradle::get_path_for_id(cache, id);
}

void
disk_cache::record_usage(int64_t id)
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    cache.usage_record_buffer.push_back(id);
}

void
disk_cache::write_usage_records()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    cradle::write_usage_records(cache);
}

void
disk_cache::do_idle_processing()
{
    auto& cache = *this->impl_;
    boost::lock_guard<boost::mutex> lock(cache.mutex);
    check_initialization(cache);

    if (!cache.usage_record_buffer.empty() &&
        (boost::posix_time::microsec_clock::local_time() -
            cache.latest_activity).total_milliseconds() > 1000)
    {
        cradle::write_usage_records(cache);
    }
}

}
