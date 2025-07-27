#pragma once
#include "test.h"
#include "common/time_util.h"
#include "sql_util/mysql_client.h"

class MySqlClient : public Test{
protected:
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    std::string table;

public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("host,a", po::value<std::string>(&host)->default_value(""), "host")
        ("user,u", po::value<std::string>(&user)->default_value(""), "user")
        ("password,p", po::value<std::string>(&password)->default_value(""), "password")
        ("database,d", po::value<std::string>(&database)->default_value(""), "database")
        ("table,t", po::value<std::string>(&table)->default_value(""), "table");;
    }

    virtual void run(){
        LOG(INFO) << "host: " << host << " user: " << user << " password: " << password << " database: " << database << " table: " << table;

        sql_util::MySqlClient client;
        if(!client.init(host, user, password, database)){
            return ;
        }
        std::string sql_str;
        int error_code;
        while(std::getline(std::cin, sql_str)){
            auto res = client.execute(sql_str, error_code);
            if(res != nullptr){
                client.print_result(res.get());
            }
        }

        // 创建连接
        // sql::Driver *driver;
        // std::unique_ptr<sql::Connection> con;
        // std::unique_ptr<sql::Statement> stmt;
        // std::unique_ptr<sql::ResultSet> res;
        // try {
        //     // 获取MySQL驱动实例
        //     driver = get_driver_instance();
            
        //     // 连接到MySQL服务器 (替换为您的数据库信息)
        //     con.reset(driver->connect(host, user, password));
            
        //     // 选择数据库
        //     con->setSchema(database);
            
        //     // 创建一个Statement对象，用于执行SQL查询
        //     stmt.reset(con->createStatement());
        //     stmt->execute("SET SESSION wait_timeout=86400");

        // } catch (sql::SQLException &e) {
        //     LOG(ERROR) << "exception: " << e.what() << " ,code = " << e.getErrorCode() << " ,state = " << e.getSQLState();
        //     return ;
        // }
    }

    void print_result(sql::ResultSet* res, 
        std::ostream& outputStream = std::cout, 
        const std::string& delimiter = ",", 
        bool includeHeader = true) {
        if (!res) {
            std::cerr << "错误: 结果集为空" << std::endl;
            return;
        }

        try {
        // 获取元数据
        sql::ResultSetMetaData* meta = res->getMetaData();
        int columnCount = meta->getColumnCount();

        // 打印表头 (列名)
        if (includeHeader) {
            for (int i = 1; i <= columnCount; i++) {
                outputStream << meta->getColumnName(i);
                if (i < columnCount) {
                    outputStream << delimiter;
                }
            }
            outputStream << std::endl;
        }

        // 打印数据行
        while (res->next()) {
            for (int i = 1; i <= columnCount; i++) {
                if (res->isNull(i)) {
                    // 空值处理
                    outputStream << "";
                } else {
                    // 获取字符串值并处理特殊字符
                    std::string value = res->getString(i);
                    
                    // CSV 格式处理: 如果字符串包含分隔符、引号或换行符，需要用引号包围
                    bool needQuotes = (value.find(delimiter) != std::string::npos) || 
                                        (value.find('"') != std::string::npos) || 
                                        (value.find('\n') != std::string::npos);
                
                    if (needQuotes) {
                        outputStream << '"';
                        // 将字符串中的双引号替换为两个双引号 (CSV 转义规则)
                        size_t pos = 0;
                        while ((pos = value.find('"', pos)) != std::string::npos) {
                            value.replace(pos, 1, "\"\"");
                            pos += 2;
                        }
                        outputStream << value;
                        outputStream << '"';
                    } else {
                        outputStream << value;
                    }
                }
                if (i < columnCount) {
                outputStream << delimiter;
                }
            }
            outputStream << std::endl;
        }

        } catch (sql::SQLException &e) {
            LOG(ERROR) << "sql exception: " << e.what() << " ,code = " << e.getErrorCode() << " ,state = " << e.getSQLState();
        } catch (std::exception &e) {
            LOG(ERROR) << "std exception: " << e.what();
        }
    }
};