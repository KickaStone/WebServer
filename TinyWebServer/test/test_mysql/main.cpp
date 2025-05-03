#include <iostream>
#include <mysql/mysql.h>

int main() {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        std::cout << "Error: " << mysql_error(conn) << std::endl;
        return 1;
    }
    conn = mysql_real_connect(conn, "172.18.0.2", "root", "123456", "testdb", 3306, NULL, 0);
    if (conn == NULL) {
        std::cout << "Error: " << mysql_error(conn) << std::endl;
        return 1;
    }
    std::cout << "Connected to MySQL" << std::endl;
    mysql_close(conn);
    return 0;
}