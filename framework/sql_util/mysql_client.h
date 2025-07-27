#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

namespace sql_util {

//不是线程安全的
class MySqlClient{
    public:
        MySqlClient(){}
        ~MySqlClient(){}
    
        bool init(const std::string &host, const std::string &user, const std::string &password, const std::string &database, int connect_timeout_s = 10);
        
        //Query execution was interrupted, maximum statement execution time exceeded ,code = 3024
        //Lost connection to MySQL server during query ,code = 2013 ,state = HY000
        //Can't connect to MySQL server on '127.0.0.1' (111) ,code = 2003 ,state = HY000
        std::unique_ptr<sql::ResultSet> execute(const std::string &sql_str, int &error_code, uint32_t timeout_ms = 10000);

        static void print_result(sql::ResultSet* res, 
            std::ostream& outputStream = std::cout, 
            const std::string& delimiter = ",", 
            bool includeHeader = true);
    
    private:
        bool connect();
        std::unique_ptr<sql::ResultSet>  execute_interal(const std::string &sql_str, int &error_code, uint32_t timeout_ms);

        std::string host_;
        std::string user_; 
        std::string password_;
        std::string database_;
        int connect_timeout_s_;
        sql::Driver *driver_;
        std::unique_ptr<sql::Connection> con_;
        std::unique_ptr<sql::Statement> stmt_;
        std::unique_ptr<sql::ResultSet> res;
};
}
