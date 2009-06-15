/*
    Copyright 2008-2009, Felspar Co Ltd. http://fost.3.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <fost/db>
#include <fost/test>
#include <fost/exception/transaction_fault.hpp>
#include <boost/assign/list_inserter.hpp>


using namespace fostlib;


namespace {


    const setting< string > read_dsn( L"fost-postgres-test/basic.cpp", L"Postgres tests", L"Read connection", L"pqxx/user=Test password=tester host=localhost", true );
    const setting< string > write_dsn( L"fost-postgres-test/basic.cpp", L"Postgres tests", L"Read connection", L"pqxx/user=Test password=tester host=localhost", true );


}


FSL_TEST_SUITE( db_pqxx );


FSL_TEST_FUNCTION( connect_database ) {
    dbconnection dbc1( read_dsn.value() + L" dbname=postgres" );
    dbconnection dbc2( read_dsn.value() + L" dbname=postgres", write_dsn.value() + L" dbname=postgres" );
}


FSL_TEST_FUNCTION( normal_statements ) {
    const setting< bool > commit_count( L"fost-postgres-test/basic.cpp", dbconnection::c_commitCount, false );

    dbconnection dbc( read_dsn.value() + L" dbname=postgres", write_dsn.value() + L" dbname=postgres" );

    recordset rs1( dbc.query( L"SELECT 1 WHERE 1=0" ) );
    FSL_CHECK( rs1.eof() );
    FSL_CHECK_EQ( rs1.fields(), 1 );
    FSL_CHECK_EQ( rs1.command(), L"SELECT 1 WHERE 1=0" );

    recordset rs2( dbc.query( L"SELECT 1234 AS c0" ) );
    FSL_CHECK( !rs2.eof() );
    FSL_CHECK_EQ( coerce< int >( rs2.field( 0 ) ), 1234 );
    FSL_CHECK_EQ( coerce< int >( rs2.field( L"c0" ) ), 1234 );
    FSL_CHECK_EQ( rs2.name( 0 ), L"c0" );

    recordset databases( dbc.query( L"SELECT * FROM pg_catalog.pg_database WHERE datname='FSL_Test'" ) );
    if ( databases.eof() ) {
        dbc.create_database( L"FSL_Test" );
        dbconnection dbc( read_dsn.value() + L" dbname=FSL_Test", write_dsn.value() + L" dbname=FSL_Test" );

        fostlib::meta_instance test( L"test" );
        test
            .primary_key( L"id", L"integer" )
            .field( L"name", L"varchar", false, 128 );

        dbtransaction transaction( dbc );
        transaction.create_table( test );
        transaction.commit();
    }
    FSL_CHECK( !dbc.in_transaction() );
}


FSL_TEST_FUNCTION( transaction_safeguards ) {
    const setting< bool > commit_count( L"fost-postgres-test/basic.cpp", dbconnection::c_commitCount, false );

    dbconnection dbc( read_dsn.value() + L" dbname=FSL_Test", write_dsn.value() + L" dbname=FSL_Test" );
    {
        dbtransaction transaction( dbc );
        transaction.execute( L"DELETE FROM test" );
        transaction.commit();
    }

    { // Check we can't reuse a committed transaction
        dbtransaction transaction( dbc );

        FSL_CHECK_EXCEPTION( fostlib::dbtransaction t2( dbc ), fostlib::exceptions::transaction_fault& );

        transaction.execute( L"INSERT INTO test VALUES (1, 'Hello')" );
        transaction.commit();
        FSL_CHECK_EXCEPTION( transaction.execute( L"INSERT INTO test VALUES (2, 'Hello')" ), fostlib::exceptions::transaction_fault& );
    }
    FSL_CHECK_EQ( coerce< int >( dbc.query( L"SELECT COUNT(id) FROM test" ).field( 0 ) ), 1 );

    { // Check that a duplicate key results in an error
        dbtransaction transaction( dbc );
        FSL_CHECK_EXCEPTION( transaction.execute( L"INSERT INTO test VALUES (1, 'Hello')" ), fostlib::exceptions::transaction_fault& );
    }
    FSL_CHECK_EQ( coerce< int >( dbc.query( L"SELECT COUNT(id) FROM test" ).field( 0 ) ), 1 );

    { // Check that a dropped transaction is aborted
        dbtransaction transaction( dbc );
        transaction.execute( L"INSERT INTO test VALUES (2, 'Goodbye')" );
    }
    FSL_CHECK_EQ( coerce< int >( dbc.query( L"SELECT COUNT(id) FROM test" ).field( 0 ) ), 1 );

    { // Check that a transaction is isolated
        dbtransaction transaction( dbc );
        transaction.execute( L"INSERT INTO test VALUES (2, 'Goodbye')" );

        dbconnection cnx( read_dsn.value() + L" dbname=FSL_Test" );
        FSL_CHECK_EQ( coerce< int >( cnx.query( L"SELECT COUNT(id) FROM test" ).field( 0 ) ), 1 );
        transaction.commit();
    }
    FSL_CHECK_EQ( coerce< int >( dbc.query( L"SELECT COUNT(id) FROM test" ).field( 0 ) ), 2 );

    recordset rs( dbc.query( L"SELECT id, name FROM test ORDER BY id ASC" ) );
    FSL_CHECK_EQ( coerce< int >( rs.field( 0 ) ), 1 );
    FSL_CHECK_EQ( coerce< string >( rs.field( 1 ) ), L"Hello" );
    rs.moveNext();
    FSL_CHECK_EQ( coerce< int >( rs.field( 0 ) ), 2 );
    FSL_CHECK_EQ( coerce< string >( rs.field( 1 ) ), L"Goodbye" );
    rs.moveNext();
    FSL_CHECK( rs.eof() );
}
