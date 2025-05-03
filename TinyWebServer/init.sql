drop database if exists testdb;
create database testdb;
use testdb;

CREATE TABLE user(
    username char(50) NULL,
    passwd char(50) NULL
)ENGINE=InnoDB;

INSERT INTO user(username, passwd) VALUES("test", "123456");