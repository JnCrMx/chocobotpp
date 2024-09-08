module;

#include <pqxx/pqxx>

export module pqxx;

export namespace pqxx {
    using pqxx::connection;
    using pqxx::transaction_base;
    using pqxx::nontransaction;
    using pqxx::work;
    using pqxx::result;
    using pqxx::row;

    using pqxx::unexpected_rows;

    using pqxx::zview;
    using pqxx::string_traits;
    using pqxx::no_null;
    using pqxx::nullness;
}
