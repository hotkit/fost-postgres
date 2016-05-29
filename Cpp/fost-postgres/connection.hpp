/*
    Copyright 2015, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/pg/connection.hpp>
#include <pqxx/connection>
#include <pqxx/transaction>


struct fostlib::pg::connection::impl {
    pqxx::connection pqcnx;
    using transaction_type = pqxx::transaction<pqxx::serializable>;
    std::unique_ptr<transaction_type> trans;

    json configuration;

    impl(const fostlib::utf8_string &dsn)
    : pqcnx(dsn.underlying()),
        trans(new transaction_type(pqcnx)),
        configuration(dsn.underlying().c_str())
    {
    }

    impl(const std::pair<fostlib::utf8_string, fostlib::json>  &dsn)
    : pqcnx(dsn.first.underlying()),
        trans(new transaction_type(pqcnx)),
        configuration(dsn.second)
    {
    }
};


